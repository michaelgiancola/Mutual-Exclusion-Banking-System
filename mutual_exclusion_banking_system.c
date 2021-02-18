/*Author: Michael Giancola
 *Date: 29/11/2019
 *Description: This file contains a program that will use a mutual exclusion algorithm for a bank scenario
 where many depositors and clients are able to process transactions to and from bank accounts concurrently.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

//function prototypes(the first two are the thread start routines)
void *makeDeposits(void *depositor);
void *makeTransactions(void *client);
void deposit(int accountNum, int amount, int transFeeType);
void withdraw(int accountNum, int amount, int transFeeType);
void transfer(int giveAccountNum, int takeAccountNum, int amount);

//represents a bank account
typedef struct account{		
	int accountNum;		
	char *type;		//either a Personal or Business bank account
	int depositFee;		
	int withdrawFee;
	int transferFee; 
	int transactionNum;	//transaction number limit before additional fee is added to each transaction
	int additionalFee;	//fee for when the trasnaction number is exceeded
	int overdraftFee;	//fee for when overdraft is in effect
	int overdraft;		//1 if overdraft exists on account and 0 if it does not
	int balance;		//account balance
	int numberOfAccTrans;	//number of transactions made on account
} Acc;

//generic transaction made on an account
typedef struct transaction{
	char transType;		//type of transaction occuring, d:deposit, w:withdraw, t:transfer
        int amount;		//amount associated with the transaction
        int takeAccountNum;	//account number relevant for a withdraw or part of a transfer(sender)
        int giveAccountNum;	//account number relevent for a deposit or part of a transfer(receiver)
} Trans;


//client that can make transactions on accounts
typedef struct client{
	int clientNum;	
	int numOfTrans;		//number of transactions a client makes
	Trans *transactions;	//pointer to a dynmically allocated array of transaction objects
} Cli;

//depositor that deposits money into accounts before clients have access
typedef struct depositor{
	int depositorNum;
	int numOfTrans;		//number of transactions a depositor makes
	Trans *transactions;	//pointer to a dynamically allocated array of transaction objects
} Depo;

Acc *accounts;			//pointer to an array of account objects which will be the accounts used in the bank
pthread_mutex_t lock;		//global mutex lock used to mutually exclude in critical sections of program

int main(){

	FILE *fp;							//input file pointer
	FILE* output_fp;						//output file pointer
	char* filename = "assignment_3_input_file.txt";			//name of input file
	fp = fopen(filename, "r");

	if(fp == NULL){								//check to see if input file was unable to open			
		printf("File %s could not be opened", filename);
		fprintf(output_fp,"File %s could not be opened", filename);	//print to output file as well pointed at by output_fp
		return 1;
	}

	output_fp = fopen("assignment_3_output_file.txt", "w");			//open output file with writing permissions on

	if(output_fp == NULL){							//check to see if output file was unable to open/create
		printf("Output file could not be opened");
		fprintf(output_fp,"Output file could not be opened");
		return 1;
	}
	
	int accountCount = 0;		//count the number of accounts 
	int depositorCount = 0;		//count the number of depositors
	int clientCount = 0;		//count the number of clients
	char check;			//check character for input file extraction
	
	/*loop to determine the number of accounts, depositors and clients in input file*/	
	while(1){			
		check = fgetc(fp);
		if (check == EOF){
			break;
		}
		else if(check  == 'Y' || check == 'N'){		//looks for Y or N which each account has only once in input file
			accountCount++;
		}
		else if(check == 'd'){				//looks for 'd' followed by an 'e' which denotes a depositor
			check = fgetc(fp);
			if(check == 'e'){
				depositorCount++;
			}
		}
		else if(check == 'c'){				//looks for a 'c' which denots a client
			check = fgetc(fp);
			if(check != 't'){
				clientCount++;
			}
		}	
	}
	
	fclose(fp);						//close input file

	accounts = (Acc*)malloc(sizeof(Acc)*accountCount);			//dynamically allocated accounts array using the account count found above
	Depo *depositors = malloc(sizeof(Depo)*depositorCount);			//dynamically allocated depositors array using the depositor count above
	Cli *clients = malloc(sizeof(Cli)*clientCount);				//dynamically allocaed clients array using the client count found above

	fp = fopen(filename, "r");						//reopen input file

        if(fp == NULL){
                printf("File %s could not be opened", filename);
                fprintf(output_fp,"File %s could not be opened", filename);     //print to output file as well pointed at by output_fp
                return 1;
        }

	int value;
	int i;
	int j;
	char c;

	/*loop through all accounts in input file and extract its characteristics*/
	for(i = 0; i < accountCount; i++){	
		Acc obj;
		obj.accountNum = i+1;
		obj.balance = 0;
		obj.numberOfAccTrans = 0;

		while(fgetc(fp) != ' '){
			fgetc(fp);
		}
		
		j = 0;
		while(j<=4){
			fgetc(fp);
			j++;
		}
		
		if(fgetc(fp) == 'b'){		//account type
			obj.type = "business";
		}

		else{
			obj.type = "personal";
		}
		
		j = 0;
                while(j<10){
                        fgetc(fp);
                        j++;
                }
		
		fscanf(fp, "%d", &value);	//extract deposit fee
		obj.depositFee = value;
		
		fgetc(fp);
		fgetc(fp);
		fgetc(fp);

                fscanf(fp, "%d", &value);	//extract withdraw fee
		obj.withdrawFee = value;
		
		fgetc(fp);
		fgetc(fp);
		fgetc(fp);

                fscanf(fp, "%d", &value);	//extract transfer fee
		obj.transferFee = value;
					
		j = 0;
                while(j<14){
                        fgetc(fp);
                        j++;
                }

		fscanf(fp, "%d", &value);
		obj.transactionNum = value;	//extract the additionalFee limit
		
		fgetc(fp);

		fscanf(fp, "%d", &value);
		obj.additionalFee = value;	//extract the additional fee

		j = 0;
                while(j<11){
                        fgetc(fp);
                        j++;
                }

		fscanf(fp, "%c", &c);

		if(c == 'N'){
			obj.overdraft = 0;

			accounts[i] = obj;
			fgetc(fp);
			continue;			//if there is no overdraft associated with account move onto the next account
		}
		else{					//extract the overdraft characteristic, the overdraft fee
			fgetc(fp);
			fscanf(fp, "%d", &value);
			obj.overdraft = 1;
			obj.overdraftFee = value;
			accounts[i] = obj;
			fgetc(fp);
		}	
	}
	
	int transactionCount = 0;		//keeps track of the number of transactions the depositors or clients have

	/*for each depositor create a dynamic array of transactions after counting how many*/
	for(i = 0; i < depositorCount; i++){	
		transactionCount = 0;
		fgetc(fp);		//skip over first d for depositor
		while(1){		//loop until new line character while counting the number of transactions in the depositor line
			check = fgetc(fp);

			if(check == 'd' || check == 'w' || check == 't'){
				transactionCount++;
			}

			if(check == '\n'){
				break;
			}
		}

		Depo obj;
		Trans *depTransactions= malloc(sizeof(Trans)*transactionCount);		//dynamically allocated transaction array to store depositor transactions
		obj.depositorNum = i+1;
		obj.transactions = depTransactions;
		obj.numOfTrans = transactionCount;
		depositors[i] = obj;
	}
	
	/*for each client create a dynamic array of transactions after counting how many*/
	for(i = 0; i < clientCount; i++){
		transactionCount = 0;
		while(1){
			check = fgetc(fp);

			if(check == 'd' || check == 'w' || check == 't'){
                                transactionCount++;
                        }

                        if(check == '\n' || check == EOF){
                                break;
                        }
		}
		
		Cli obj;
		Trans *cliTransactions = malloc(sizeof(Trans)*transactionCount);	//dynamically allocated client array to store client transactions
		obj.clientNum = i+1;
		obj.transactions = cliTransactions;
		obj.numOfTrans = transactionCount;
		clients[i] = obj;
	}

	fclose(fp);			//close input file
	
	fp = fopen(filename, "r");	//reopen input file for a third time

        if(fp == NULL){
                printf("File %s could not be opened", filename);
                fprintf(output_fp,"File %s could not be opened", filename);     //print to output file as well pointed at by output_fp
                return 1;
        }
	
	i = 0;

	//move the input file pointer to point at the first depositor line in the input file
	while(i < accountCount){
		check = fgetc(fp);
		if(check == '\n'){
			i++;
		}
	}
	
	/*extract depositor charactersitics from input file*/
	for(i = 0; i < depositorCount; i++){
                fgetc(fp);              //skip over first d for dep
		j = 0;
                while(1){
                        check = fgetc(fp);

                        if(check == 'd'){
				depositors[i].transactions[j].transType = 'd';
				fgetc(fp);
				fgetc(fp);
				fscanf(fp, "%d", &value);
				depositors[i].transactions[j].giveAccountNum = value;
				fgetc(fp);
				fscanf(fp, "%d", &value);
				depositors[i].transactions[j].amount = value;
				j++;
                        }

                        if(check == '\n'){
                                break;
                        }
                }
	}
	
	/*extract client charactersitics from the input file*/
	for(i = 0; i < clientCount; i++){		
		j = 0;
                while(1){
                        check = fgetc(fp);

                        if(check == 'd'){						//if the transaction is a deposit
                                clients[i].transactions[j].transType = 'd';
                                fgetc(fp);
                                fgetc(fp);
                                fscanf(fp, "%d", &value);
                                clients[i].transactions[j].giveAccountNum = value;
                                fgetc(fp);
                                fscanf(fp, "%d", &value);
                                clients[i].transactions[j].amount = value;
                                j++;
                        }

                        else if(check == 'w'){						//if the transaction is a withdraw
                                clients[i].transactions[j].transType = 'w';
                                fgetc(fp);
                                fgetc(fp);
                                fscanf(fp, "%d", &value);
                                clients[i].transactions[j].takeAccountNum = value;
                                fgetc(fp);
                                fscanf(fp, "%d", &value);
                                clients[i].transactions[j].amount = value;
                                j++;
                        }

                        else if(check == 't'){						//if the transaction is a transfer
                                clients[i].transactions[j].transType = 't';
                                fgetc(fp);
                                fgetc(fp);
                                fscanf(fp, "%d", &value);
                                clients[i].transactions[j].takeAccountNum = value;
                                fgetc(fp);
                                fgetc(fp);
                                fscanf(fp, "%d", &value);
                                clients[i].transactions[j].giveAccountNum = value;
                                fgetc(fp);
                                fscanf(fp, "%d", &value);
                                clients[i].transactions[j].amount = value;
                                j++;
                        }

                        if(check == '\n' || check == EOF){
                                break;	//break out of loop if the end of file is reached
                        }
                }
	}
	
	fclose(fp);			//close input file

	//create the threading functions and their calls
	int err_thread;
	
	pthread_t *threads = malloc(sizeof(pthread_t)*depositorCount);	//dynamic array of threads for the number of depositors 
	pthread_t *threads1 = malloc(sizeof(pthread_t)*clientCount);	//dynamic array of threads for the number of clients
	
	//mutex lock validation
	if (pthread_mutex_init(&lock, NULL) != 0)
    	{
        	printf("\n mutex init failed\n");
        	return 1;
   	} 

	/*create threads for each depositor using the thread routine makeDeposites*/
	for(i = 0; i < depositorCount; i++){
		err_thread = pthread_create(&threads[i], NULL, &makeDeposits, &depositors[i]);	
		if(err_thread != 0){	//check if thread is created successfully
			printf("\n Error creating thread %d", i);
		}
	}
	
	//join all of the depositor threads and makes sure that depositors are done before client threads begin
	for (i = 0; i< depositorCount; i++)
		pthread_join(threads[i], NULL); 
	
	/*create threads for each client using the thread routine makeTransactions*/
	 for(i = 0; i < clientCount; i++){
                err_thread = pthread_create(&threads1[i], NULL, &makeTransactions, &clients[i]);
                if(err_thread != 0){	//check if the thread is created successfully
                        printf("\n Error creating thread %d", i);
                }
        }
	
	 //join all of the client threads
	for (i = 0; i< clientCount; i++)
                pthread_join(threads1[i], NULL);

	pthread_mutex_destroy(&lock); 	//destroy the mutex lock from program

	/*print out the account, along with its type and balance*/
	for (i = 0; i < accountCount; i++){
		printf("a%d type %s %d\n", accounts[i].accountNum, accounts[i].type, accounts[i].balance);
		fprintf(output_fp, "a%d type %s %d\n", accounts[i].accountNum, accounts[i].type, accounts[i].balance);
	}

	fclose(output_fp); //closes output file

	/*deallocate the memory for each dynamic transaction array associated wit each  depositor*/
	for(i = 0; i < depositorCount; i++){
		free(depositors[i].transactions);
	}

	/*deallocate the memory for each dynamic transaction array associated with each client*/
	for(i = 0; i < clientCount; i++){
		free(clients[i].transactions);
	}
	
	//free the memory for remaining five dynamically allocated arrays
	free(accounts);
	free(depositors);
	free(clients);
	free(threads);
	free(threads1);

	return 0;
	
}//main end

/*thread routine for depositors to process transactions concurrently*/
void *makeDeposits(void *depositor){
	
	//must cast to be able to use depositor that was passed as an argument
	Depo *threadDepositor = (Depo*)depositor;

	int i;
	for(i = 0; i < threadDepositor->numOfTrans; i++){	//loop through all of the depositor's transactions
		pthread_mutex_lock(&lock);  // ENTRY REGION
		deposit(threadDepositor->transactions[i].giveAccountNum, threadDepositor->transactions[i].amount, accounts[threadDepositor->transactions[i].giveAccountNum - 1].depositFee);	//critical region since we are manipulating the values of the accounts which are global
		pthread_mutex_unlock(&lock); // EXIT REGION
	}
}

/*thread routine for clients to process transactions concurrently*/
void *makeTransactions(void *client){
	//must cast to be able to use client
	Cli *threadClient = (Cli*)client;

	int i;
        for(i = 0; i < threadClient->numOfTrans; i++){	//loop through all of the client's transactions

                pthread_mutex_lock(&lock);  // ENTRY REGION
		
		//critical region
		if (threadClient->transactions[i].transType == 'd'){		//if the transaction is a deposit call the deposit functin
			 deposit(threadClient->transactions[i].giveAccountNum, threadClient->transactions[i].amount, accounts[threadClient->transactions[i].giveAccountNum - 1].depositFee);
		}

		else if (threadClient->transactions[i].transType == 'w'){	//if the transaction is a withdraw call the withdraw function
			withdraw(threadClient->transactions[i].takeAccountNum, threadClient->transactions[i].amount, accounts[threadClient->transactions[i].takeAccountNum - 1].withdrawFee);
		}	

		else if (threadClient->transactions[i].transType == 't'){	//if the transaction is a transfer call the transfer function
			 transfer(threadClient->transactions[i].giveAccountNum, threadClient->transactions[i].takeAccountNum, threadClient->transactions[i].amount);
		}

                pthread_mutex_unlock(&lock); // EXIT REGION
        }

}

/*deposit deposits the argument amount into the account with the 
 * argument accountNum and applys the value transFeeType argument to the account*/
void deposit(int accountNum, int amount, int transFeeType){

	int currTransactionNumber = accounts[accountNum - 1].numberOfAccTrans;
	int tempBalance;
	int tempInitialBalance;

	if (accounts[accountNum - 1].overdraft == 0){		//if there is no overdraft associated with the account
		tempBalance = accounts[accountNum - 1].balance;
		if (currTransactionNumber+1 > accounts[accountNum - 1].transactionNum){	//check if the additional fee should be applied to transaction
			  tempBalance -= accounts[accountNum - 1].additionalFee;
		}

		tempBalance += amount;		//add amount to the account balance
		tempBalance -= transFeeType;	//subtract the fee from the account balance

		if (tempBalance >= 0){		//if the balance >= 0 after the transaction then process it; if it is negative then do not process it
			accounts[accountNum - 1].numberOfAccTrans++;
			accounts[accountNum - 1].balance = tempBalance;
		}
	}

	else{									//if the account has overdraft protection
		tempInitialBalance = accounts[accountNum - 1].balance;
		tempBalance = accounts[accountNum - 1].balance;
		tempBalance += amount;
		tempBalance -= transFeeType;
	
		if(currTransactionNumber+1 > accounts[accountNum - 1].transactionNum){	//check if the additional fee should be applied
			tempBalance -= accounts[accountNum - 1].additionalFee;
		}

		if(tempBalance < 0){	//this is where you must implement if u should charge overdraft or not
			int j = -500;
			int i = -1;
				
			while (tempInitialBalance < j){	//figure out what range the balance is in before the transaction is applied
				i -= 500;	
				j -= 500;
			}
			
			while (tempBalance < j){	//figure out the difference in value between the inital range and the new range of the balance
				 if(j == -5000){	//if j is -5000 then overdraft limit has been exceeded and do not process transaction(balance < -5000)
                                        return;
                                }

				tempBalance -= accounts[accountNum - 1].overdraftFee;	//each iteration of the loop apply overdraft fee
				j -= 500;						//update value of j and i
				i -= 500;
			}

			 if(tempInitialBalance > 0 && tempBalance < 0){			//this is the case for if the balance goes from a positive value to a negative one caused by the current transaction
                                tempBalance -= accounts[accountNum - 1].overdraftFee;
                        }
		}
		accounts[accountNum - 1].balance = tempBalance;	//set the balance of the account to the temp balance calculated
		accounts[accountNum - 1].numberOfAccTrans++;	//increment the number of transactions made using the account
	}
}

/*withdraw function removes the value of the amount argument from the account
 * with the account number accountNum and applys the value of transFeeType to the account*/
void withdraw(int accountNum, int amount, int transFeeType){
	
	int currTransactionNumber = accounts[accountNum - 1].numberOfAccTrans;
	int tempBalance;
	int tempInitialBalance;

        if (accounts[accountNum - 1].overdraft == 0){           //if there is no overdraft associated with the account
                tempBalance = accounts[accountNum - 1].balance;
                if (currTransactionNumber+1 > accounts[accountNum - 1].transactionNum){
                          tempBalance -= accounts[accountNum - 1].additionalFee;
                }

                tempBalance -= amount;
                tempBalance -= transFeeType;

                if (tempBalance >= 0){
                        accounts[accountNum - 1].numberOfAccTrans++;
                        accounts[accountNum - 1].balance = tempBalance;
                }
        }

	else{                                                                   //if the account has overdraft protection
                tempInitialBalance = accounts[accountNum - 1].balance;
		tempBalance = accounts[accountNum - 1].balance;
		tempBalance -= amount;
                tempBalance -= transFeeType;

                if(currTransactionNumber+1 > accounts[accountNum - 1].transactionNum){
                        tempBalance -= accounts[accountNum - 1].additionalFee;
                }

                if(tempBalance < 0){       //this is where you must implement if u should charge overdraft or not (SAME CHECK AS DEPOSIT FUNCTION ABOVE)
                        int j = -500;
                        int i = -1;

                        while (tempInitialBalance < j){ //gets you the range of overdraft for your inital balance
                                i -= 500;
                                j -= 500;
                        }

                        while (tempBalance <  j){

				if(j == -5000){		//if overdraft limit is exceeded then do not process transaction (balance < -5000)
                                        return;
                                }

                                tempBalance -= accounts[accountNum - 1].overdraftFee;
                                j -= 500;
				i -= 500;
                        }

			if(tempInitialBalance > 0 && tempBalance < 0){	//case where balance goes from positive to negative after transaction is applied
				tempBalance -= accounts[accountNum - 1].overdraftFee;
			}
                }
		accounts[accountNum - 1].balance = tempBalance;
                accounts[accountNum - 1].numberOfAccTrans++;
        }
}

/*transfer function transfers the value of the argument amount from the account with the account
 * number takeAccountNum and gives to the account with number takeAccountNum*/ 
void transfer(int giveAccountNum, int takeAccountNum, int amount){
	int initialBalance = accounts[takeAccountNum - 1].balance;
	withdraw(takeAccountNum, amount, accounts[takeAccountNum - 1].transferFee);	//a withdraw occurs using the takeAccountNum, value, and transferFee

	if(initialBalance == accounts[takeAccountNum - 1].balance){	//transfer not able to take place since unable to remove money from intial account
		return;
	}

	initialBalance =  accounts[giveAccountNum - 1].balance;

	deposit(giveAccountNum, amount, accounts[giveAccountNum - 1].transferFee);	//a deposit occurs using the giveAccountNum, value, and transferFee

	if (initialBalance == accounts[giveAccountNum - 1].balance){	//if receiving account is unable to process the depost transaction then refund the original sender
		accounts[takeAccountNum - 1].balance += amount;
		accounts[takeAccountNum - 1].balance += accounts[takeAccountNum - 1].transferFee;
		accounts[takeAccountNum - 1].numberOfAccTrans--;
	}
}
