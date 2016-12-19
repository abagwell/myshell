/*
Name: Andrew Bagwell

File: smallsh.c

Description: This is a small shell program that runs within a bash shall. It has three
built in commands with all other processes being forked and exec'd. This shell can handle simple
redirection as well. It handles foreground as well as background processes. 

Summary: main() calls doShell(), which runs as loop calling checkBG(), userline(), and runArgs() until a zero is returned, thereby 
terminating the loop. The runArgs function ignores commands, runs the builtin commands, or runs the commands as a process via runProcess() when required.
The runProcess() function uses fork, exec and waitpid to do the necessary process creation. It also handles the necessary redirection via the functions 
getInputRD, getOutputRD and doRedirection. 
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

//constants
#define LINE_MAX 2046
#define ARG_MAX 512

//prototypes
void doShell();
int checkBG();
char* userLine();
char** parseLine(char*);
int runArgs(char**);
int runProcess(char**);
int getInputRD(char**);
int getOutputRD(char**);
void doRedirection(char**, int, int);

//shell/ variables 

int bgFlag; //flags whether or not a command will be a background process
int status = 0; //status variable used with the status command and execsv
int argCount; // serves as a counter of the number of arguments in a user entered command
int bgCount = 0; //serves as a counter and a FLAG for bgProcess()
pid_t bgArray[100]; //an array of background pid's
struct sigaction fgIntHandleStruct; //sigaction struct needed for signal handling


//There are three built-in shell commands. All built in commands execute via a function call.

//This is the change directory function. It takes as an argument a pointer to a pointer of a char (an array of char pointers).
//It returns 1 upon success and it calls C's chdir() function in order to effect the change

int builtin_changeDir(char** argParams) {
	
	//if only the 'cd' command was passed by the user, it returns the user to their home directory
	
	if (argParams[1] == NULL) {

		chdir(getenv("HOME"));
	}

	//if there are more the cd command two arguments (2 total with cd included), there is an error and a usage function will be printed
	else if (argParams[2] != NULL)  {

		printf("command [arg1 arg2 ...] [< input_file] [> output_file] [&]\n"); 
	}

	//check to make sure that the chdir() function did not return an error

	else if (chdir(argParams[1])!= 0) {

		perror("chdir");
	}

	return 1; 
}

//This is the status function. It takes as a parameter a pointer to char pointers (an array of char pointers). 
//If the processes exited, it returns the number of the status. If it terminates, the terminating signal is returned

int builtin_status(char** argParams2) {

	if (WIFEXITED(status))
		printf("Exited with status: %d\n", WEXITSTATUS(status));
	else
		printf("terminated by signal: %d\n", status);
	
	return 1;
}

//This is the exit function. It has no parameters and returns 0 on success, which will terminate the doShell loop, closing the program.
int builtin_exit() {

	printf("Exiting shell...\n");
	return 0; 
}


// This is the main function of program is simply calls doShell() and returns 0 once completed. 

int main() {

	doShell();
	return 0; 
}

//This is the primary function of the program. It continually executes while shell is equal to 1. 

void doShell() {
	
	int shell; // loop control variable is declared here 

	do {

		//set up struct for signal handling
		fgIntHandleStruct.sa_handler = SIG_IGN;
		fgIntHandleStruct.sa_flags = 0; //http://beej.us/guide/bgipc/output/html/multipage/signals.html
		sigfillset(&(fgIntHandleStruct.sa_mask));
		sigaction(SIGINT, &fgIntHandleStruct, NULL);

		//check background process 
		checkBG();

		//get line 
		char* input = userLine();

		//parse into commands
		char** userArgs = parseLine(input);

		//execute those commands
		shell = runArgs(userArgs);

		//free memory
		free(input);
		free(userArgs);
	}

	while (shell);

}

//This function checks the background via the waitpid() funtion to see if any children processes have terminated

int checkBG() {

	//if there are no background processes executing, the function returns
	if (bgCount < 1) 
		return 1; 
	//if not, loop through the bgArray using bgCount as a limit, if a bg process has exited, it's pid and exit status is printed to screen. If it's terminated by signal it's pid and terminating signal number are printed to the screen
	else {
		int i; 
		for (i = 0; i < bgCount; i++) {

			if (waitpid(bgArray[i], &status, WNOHANG) > 0) {

				if(WIFEXITED(status))
					printf("Background Process PID: %d exited with status of %d\n", bgArray[i], WEXITSTATUS(status));

				if (WIFSIGNALED(status))
					printf("Background Process PID: %d terminated by signal %d\n", bgArray[i], WTERMSIG(status));
			}
		}
	}

	return 1; 

}

//This function gets the line entered by the user. It sets the bgFlag accordingly, prints the prompt for user, puts the user input into a buffer via getLine(), and then returns that buffer. 

char* userLine() {

	//initialize bg flag to 0
	bgFlag = 0; 
	//create the pointers for getline(), but we will let the function itself handle the necessary allocation
	char* buffer = NULL;
	size_t buffSize = 0;	//can't be negative, so I used size_t.
	
	//print prompt and flush stdout
	printf(": "); 
	fflush(stdout);

	//get the line
	getline(&buffer, &buffSize, stdin);

	//check to see if string contains & and is a background process, if so, set the bgflag.

	if (strstr(buffer, "&")) {
		bgFlag = 1;
	}

	return buffer;
}

//This function parses the user entered line into a commands via strtok() and places that parsed line into an array of arguments which, at function complete, are returned.

char** parseLine(char* buffer) {

	//create the array of pointers to char pointers, this will be returned by the function once it is filled
	char** arguments = malloc(ARG_MAX * sizeof(char*));
	
	//create char pointer that will hold the parsed line
	char* parsedLine; 
	
	//declare and initiliaze to 0 a variable that refers to the index in the array of arguments 
	int index = 0;
    
    //use strtok with delimiters of " " and \n to return the first parsed command
	parsedLine = strtok(buffer, " \n");
    
    //if there is at least 1 command, pass that command into the array of args and call strok passing the result into parsed, continue doing this until NULL is returned by strtok
	while (parsedLine != NULL) {

        arguments[index] = parsedLine;
        parsedLine = strtok(NULL, " \n");
        index = index + 1;
	}
		 
	//now add the NULL terminator in order to use execvp
	arguments[index] = NULL;

	//set the arg count equal to index, this is needed for the redirection function
	argCount = index; 

	//the return the arguments

	return arguments;
}

//This function analyzes the 1st command in arg array, performing the appropriate action.
//It returns an int which is stored in the "shell" variable of doShell(), driving the doShell loop 

int runArgs(char** argsPassed) {

	//check if args array is empty, if it is, return 1

	if (argsPassed[0] == NULL) 
		return 1; 
	
	//check if the argument is a comment, if it is return 1

	else if (strstr(argsPassed[0], "#")) 
		return 1; 
	
	//check if the command is builtin command, if it is, return the result the respective function call

	else if (strcmp(argsPassed[0], "cd") == 0) 
			return builtin_changeDir(argsPassed);
	
	else if (strcmp(argsPassed[0], "exit") == 0) 
			return builtin_exit();

	else if (strcmp(argsPassed[0], "status") == 0) 
			return builtin_status(argsPassed);

	//If not an ignored value or a built in command, we run this as a process via a call to runProcess()

	else {
		
		runProcess(argsPassed);
	}

}

//This function runs the non-builtin commands for the shell via fork() and execpv(). It takes the argument array as a parameter and returns 1 to runArgs if successful.
// A switch statement determines the flow of the function depending on the value of the bgFlag. 
//If the bg flag is set, the sigaction struct's handler is reset to default, so SIGINT is not ignored for fg children.
// The function also calls helper functions (getInputRD, getOutputRD, and doRedirection) in order to perform the necessary redirection

int runProcess(char** argsPassed) {

	switch(bgFlag){

		pid_t spawnpid = -1; 
		
		//if bgFlag is not set, we run this command as a foreground process
		case 0: 

			//call fork to begin process creation and catch its return values in spawnpid
			spawnpid = fork();

			if (spawnpid < 0) {
				perror("fork");
				exit(EXIT_FAILURE);
			}

			//this is the child
			else if (spawnpid == 0) {

				//Change sa_handler to DEF because we don't want to ignore SIGINTs for fg children
				fgIntHandleStruct.sa_handler = SIG_DFL; 
				sigaction(SIGINT, &fgIntHandleStruct, NULL);

				//determine if the arguments contain any redirection symbols..see below for function descriptions
				int inputRed = getInputRD(argsPassed);
				int outputRed = getOutputRD(argsPassed);

				//perform the necessary redirection
				doRedirection(argsPassed, inputRed, outputRed);

				//call execvp to transform process into program, catch error if it occurs
				if (execvp(argsPassed[0], argsPassed) == -1) {
					perror("execvp");
					exit(EXIT_FAILURE); 
				}
			}

			//this is the parent process
			else {

				//wait for the fg child process to change state
				waitpid(spawnpid, &status, 0);
			}

			break; 

		//if bg flag is set, this is a background process
		case 1: 

			//fork the process
			spawnpid = fork();

			if (spawnpid < 0) {
				perror("fork");
				exit(EXIT_FAILURE);
			}

			// this is the child 
			else if (spawnpid == 0) {

				//determine if the arguments contain any redirection symbols..see below for function descriptions
				int inputRed = getInputRD(argsPassed);
				int outputRed = getOutputRD(argsPassed);

				//determine if the arguments contain any redirection symbols..see below for function descriptions
				doRedirection(argsPassed, inputRed, outputRed);

				//call execvp to transform process into program, catch error if it occurs
				if (execvp(argsPassed[0], argsPassed) == -1) {
					perror("execvp");
					exit(EXIT_FAILURE); 
				}
			}

			//this is the parent and we don't wait for the child process here instead we use checkBG() above
			else {

				printf("Background process, pid: %d created.\n", spawnpid);

				//place the bg child pid in the bg array
				bgArray[bgCount] = spawnpid;
				
				//increment the bg counter
				bgCount = bgCount + 1; 
			}

			break;
		}

	return 1; 
}


//This function takes the array of arguments passed by the user, searches within that array for the input redirection symbol, if it found, it returns the index

int getInputRD(char** uncheckedArgsIn) {

	int i = 0;

	while (uncheckedArgsIn[i] != NULL) {
		if (strcmp(uncheckedArgsIn[i], "<") == 0)
			return i;
		else 
			i = i + 1; 
	}
	return -1;
}

//This function takes the array of arguments, seraches within that array for the output redirection symbol, if it is found, it returns that index 

int getOutputRD(char** uncheckedArgsOut) {

	int i = 0;

	while (uncheckedArgsOut[i] != NULL) {
		if(strcmp(uncheckedArgsOut[i], ">") == 0)
			return i;
		else 
			i = i + 1; 
	}
	return -1;
}

//This function actually performs the necessary input and output redirection

void doRedirection(char** redirectArgs, int iRD, int oRD) {

	//check for input redirection and set up if necessary
	if (iRD > -1) {
 		
 		//open the file specified in array index past the input redirection symbol with Read Only privileges
		int fd = open(redirectArgs[iRD+1], O_RDONLY);

		//check for errors in open(),e.g, the file does not exist
		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}

		//call dup2 to redirect the stdin to the file descriptor
		int fd2 = dup2(fd, 0);

		//check for errors
		if (fd2 == -1) {
			perror("dup2");
			exit(EXIT_FAILURE);
		}

		//clean up the arguments getting passed to execv (it requires a null terminated array)
		 redirectArgs[iRD] = NULL;
		 
	}

	//check for output redirection and set up if necessary
	else if (oRD > -1) {

		//open the file specified in array index past the output redirection symbol with write only privileges, create or truncate it if necessary
		int fd = open(redirectArgs[oRD+1], O_WRONLY|O_CREAT|O_TRUNC, 0664);

		//check for errors
		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}

		//call dup2 to redirection stdout to the file descriptor
		int fd2 = dup2(fd, 1);

		//check for errors
		if (fd2 == -1) {
			perror("dup2");
			exit(EXIT_FAILURE);
		}

		//clean up the arguments array getting passed to execv
		 redirectArgs[oRD] = NULL;
		
	}

	//here, if the bg flag is set and either input or output (or both) redirection is not provided by the user, we have to redirect stdin and/or stdout to dev null for bg processes
	else if(bgFlag == 1 && (iRD < 0 | oRD < 0)) {

		//redirect stdin and stdout for the bg process to dev NULL

		int devNull = open("/dev/null", O_WRONLY);	//http://stackoverflow.com/questions/14846768/in-c-how-do-i-redirect-stdout-fileno-to-dev-null-using-dup2-and-then-redirect

		//if input redirection was not specified by user, redirect stdin to devnull
		if (iRD < 0) {

			if (devNull == -1) {
				perror("open");
				exit(EXIT_FAILURE);
			}

			int fd = dup2(devNull, 0);

			if (fd == -1) {
				perror("dup2");
				exit(EXIT_FAILURE);
			}

		}

		//if output redirection was not specified by user, redirect stdout to devnull 

		if (oRD < 0) {

			int fd2 = dup2(devNull, 1);

			if (fd2 == -1) {
				perror("dup2");
				exit(EXIT_FAILURE);
			}
		}
		
		//clean up arguments for execvp, we don't need the & anymore either. 

		redirectArgs[argCount - 1] = NULL;
	}
}
