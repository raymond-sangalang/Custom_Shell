#include <wait.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>

// Utilized for builtin - example from tlcl book for process's environment
extern char **environ;

// constants
#define MAX_INPUT 256
#define MAX_ARGS  20

// function declarations
int execute_builtins(char **);



/*** Shell
    Primary Logic:
        While(1)
            get command - parse text input
            execute command - fork process (if necessary)
            wait for command to finish
    - Shell using fork(), wait(), and execvp()
 * ***/
int main() {

    while (true) 
    {
        char command_input[MAX_INPUT], 
             *argv[MAX_ARGS];
        int count = 0;

        printf("$ ");
        fflush(stdout);

        // get command from stdin
        if (fgets(command_input, sizeof(command_input), stdin) == NULL)
            break;

        // find newline index and replace with null terminator
        command_input[strcspn(command_input, "\n")] = '\0';

        // parse text input of command into tokens, deliminated by spaces
        char *token = strtok(command_input, " ");
        while (token != NULL && count < MAX_ARGS - 1) {
            argv[count++] = token;
            token = strtok(NULL, " ");
        }
        argv[count] = NULL;

        // Validate if entry was inputed or empty
        if (argv[0] == NULL) continue;

        // Condition builtin commands
        // - no fork required
        if (execute_builtins(argv))
            continue;

        // since it is not a builtin, we fork process - request from kernel
        //  => fork -> exec -> wait


        /***  Iterate through argument values and check if pipe is a required operation ***/
        int pipe_index = -1;
        for (int index = 0; argv[index] != NULL; index++) {
            if (strcmp(argv[index], "|") == 0) {
                pipe_index = index;
                argv[index] = NULL;      // set cut off on command split
                break;
            }
        }


        if (pipe_index != -1) 
        {
            /***  There is pipes in the command - split commands ***/
            char *first_command[MAX_ARGS], *second_command[MAX_ARGS];

            // Obtain the first command
            for (int index = 0; index < pipe_index; index++)
                first_command[index] = argv[index];
            first_command[pipe_index] = NULL;

            // Obtain the second command
            int j = 0;
            for (int i = pipe_index + 1; argv[i] != NULL; i++)
                second_command[j++] = argv[i];
            second_command[j] = NULL;


            // Initialize file descriptor array
            int fd[2];

            // Kernel fills values when passing array
            pipe(fd);


            int child_pid1 = fork();               // Create first child to operate on first command
            if (child_pid1 == 0) {
                // redirect stdout to write into pipe
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);               // close both ends
                close(fd[1]);

                execvp(first_command[0], first_command);
                perror("Error: First execution failed");
                exit(1);
            }

            int child_pid2 = fork();               // Create second child to operate on the second command
            if (child_pid2 == 0) {
                // redirect stdin to read from pipe
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]);               // close both ends
                close(fd[0]);

                execvp(second_command[0], second_command);
                perror("Error: Second execution failed");
                exit(1);
            }

            // Close the file descriptors
            close(fd[0]);
            close(fd[1]);
            
            // Wait for the children/processes to exit
            waitpid(child_pid1, NULL, 0);
            waitpid(child_pid2, NULL, 0);

        } else {

            /***  There is no pipes in the command ***/
            char *input_file = NULL,
                 *output_file = NULL;
            int append = 0;

            // Parse through the argument values and check for redirection
            // remove the symbols
            for (int i = 0; argv[i] != NULL; i++) 
            {
                if (strcmp(argv[i], "<") == 0) {
                    input_file = argv[i+1];
                    argv[i] = NULL;
                }
                else if (strcmp(argv[i], ">") == 0) {
                    output_file = argv[i+1];
                    append = 0;
                    argv[i] = NULL;
                }
                else if (strcmp(argv[i], ">>") == 0) {
                    output_file = argv[i+1];
                    append = 1;
                    argv[i] = NULL;
                }
            }

            // Create child process to run the command
            int child_pid = fork();
            if (child_pid == 0) 
            {
                // input redirection of resource - file as stdin
                if (input_file) {
                    int fd = open(input_file, O_RDONLY);
                    if (fd < 0) {
                        perror("input open failed");
                        exit(1);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                // output redirection of resource - file as stdout  
                // note: trunc - overwrite
                //       man open - for the definitions of flags
                if (output_file) {
                    int flags = O_WRONLY | O_CREAT  | (append ? O_APPEND : O_TRUNC);

                    // set arguments and permissions for opening a resource
                    int fd = open(output_file, flags, 0644);
                    if (fd < 0) {
                        perror("output open failed");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                // Note execvp uses PATH lookup
                execvp(argv[0], argv);
                perror("exec failed");
                exit(1);
            }
            else 
            {   
                waitpid(child_pid, NULL, 0);  // parent waits for child
            }
        }
    }
    return 0;
}



// execute_builtins - Verifies if builtin commands are within the list of entries, executes
//                  the command, and returns status of executing a builtin command
int execute_builtins(char **argv) 
{   
    if (argv[0] == NULL) return 1;

    // exit command - kills the shell
    if (strcmp(argv[0], "exit") == 0) exit(0);
    
    // cd command - requires chdir - change the shell's working directory
    if (strcmp(argv[0], "cd") == 0) 
    {    
        if (argv[1] == NULL) {
            struct passwd *pw = getpwuid(geteuid());
            if (chdir(pw->pw_dir) != 0)
                fprintf(stderr, "cd: invalid home directory entry with no cd argument.\n");
        }
        else if (chdir(argv[1]) != 0)
                perror("cd failed");
   
        return 1;
    }

    // display_env - custom built-in using an absolute path to an executable file
    if (strcmp(argv[0], "display_env") == 0) 
    {
        char **ep;
        for (ep = environ; *ep != NULL; ep++)
            puts(*ep);
        return 1;
    }
    
    return 0;  // indicates that the command is not a builtin
}