//
// Created by Abigail on 11/2021.
//
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

// ######################################   prepare:   ################################################################
int prepare(void){
    return 0;
}

// ##################################   process_arglist:   #############################################################
int process_arglist(int count, char **arglist){

    signal(SIGINT, SIG_IGN); //ignoring SIGINT when in the shell (parent)

    int pid_child1;
    int pid_child2;
    int child_status;
    int dup_res;

    int zero = 0;
    int *is_background = &zero;
    int *is_pipe = &zero;

    int pipe_i = 0;
    int pfds[2];
    int pipe_ret = pipe(pfds);

    if(pipe_ret == -1){
        fprintf(stderr, "pipe command failed in parent %d. %s\n",getpid(), strerror(errno));
        return 0; // return 0 in case of an error (as asked in instructions).
    }

    int fork1_ret = fork(); // <-- first fork

    // ========================================= parent process: ======================================================
    if (fork1_ret > 0){
        signal(SIGCHLD,SIG_IGN); // preventing zombies from creating

        // --------- foreground process: ---------------
        if ((strcmp(arglist[count-1], "&") != 0) && (! *is_background)){
            pid_child1 = wait(&child_status); // <--- wait
            if (pid_child1 == -1 && errno != EINTR && errno != ECHILD){
                fprintf(stderr, "'wait' command failed in parent %d. %s\n",getpid(), strerror(errno));
                return 0; // return 0 in case of an error (as asked in instructions).
            }
        }
        // ------------------- pipe: --------------------
        else if (*is_pipe){
            arglist[count-2] = NULL; //  split the command by replacing "|" with NULL
            int fork2_ret = fork(); // <--- fork2
            // --- parent: ---
            if (fork2_ret > 0){
                pid_child2 = wait(&child_status); // <--- wait for child 2
                if (pid_child2 == -1 && errno != EINTR && errno != ECHILD){
                    fprintf(stderr, "'wait' command failed in parent %d. %s\n",getpid(), strerror(errno));
                    return 0; // return 0 in case of an error (as asked in instructions).
                }
            }
            // --- second child: ---
            else if (fork2_ret == 0){
                signal(SIGINT, SIG_DFL); // default disposition of SIGINT when in child process (^C terminates the process)
                close(pfds[0]); // second child is the writer, hence need to close the pipe reader
                dup_res = dup2(pfds[1], STDOUT_FILENO); // <-- dup2 (redirect output to pipe writer instead to stdout)
                if (dup_res == -1) {
                    fprintf(stderr, "Error redirecting output to pipe writer: %s\n", strerror(errno));
                    exit(1);
                }
                close(pfds[1]); // no need of the file descriptor anymore since the output is already redirected into it.
                execvp(arglist[0], arglist);
                // will get here only if the execvp failed:
                fprintf(stderr, "exec of child %d failed. %s\n",getpid(), strerror(errno));
                exit(1);
            }
            // fork2 error:
            else{
                fprintf(stderr, "fork failed\n.");
                return 0; // return 0 in case of an error (as asked in instructions).
            }
        }
    }
    // ============================================ first child: ====================================================
    else if (fork1_ret == 0){
        signal(SIGINT, SIG_DFL); // default disposition of SIGINT when in child process (^C terminates the process)

        // loop over the arglist to check if it's a pipe command:
        if (count >= 3){
            for (pipe_i = 1; pipe_i < count - 1; pipe_i++){
                if (strcmp(arglist[pipe_i], "|") == 0){
                    *is_pipe = 1;
                }
            }
        }

        // ------------------- pipe: -------------------------
        if (*is_pipe){
            close(pfds[1]); // first child is the reader, hence need to close the pipe writer
            dup_res = dup2(pfds[0], STDIN_FILENO); // <-- dup2 (change input source to pipe reader instead stdin)
            if (dup_res == -1) {
                fprintf(stderr, "Error changing input source to pipe reader: %s\n", strerror(errno));
                exit(1);
            }
            close(pfds[0]); // no need of the file descriptor anymore since the input is already redirected into it.
            execvp(arglist[pipe_i + 1], arglist + pipe_i + 1);
            // will get here only if the execvp failed:
            fprintf(stderr, "exec of child %d failed. %s\n",getpid(), strerror(errno));
            exit(1);

        }
        // --------------- Redirect output: --------------------
        else if (count >= 3 &&  strcmp(arglist[count-2], "<") == 0){
            int fd = open(arglist[count - 1], O_WRONLY); // <-- open
            if (fd < 0) {
                fprintf(stderr, "Error opening file: %s\n", strerror(errno));
                exit(1);
            }
            dup_res = dup2(fd, STDOUT_FILENO); // <-- dup2 (redirecting the output into the file (instead of stdout)
            if (dup_res == -1) {
                fprintf(stderr, "Error redirecting output to file: %s\n", strerror(errno));
                exit(1);
            }
            close(fd); // no need of the file descriptor anymore since the output is already redirected into it.
            arglist[count-2] = NULL; // removing the "<" sign and filename from the arglist in order to pass it to execvp
            execvp(arglist[0], arglist); // <-- execvp
            // will get here only if the execvp failed:
            fprintf(stderr, "exec of child %d failed. %s\n",getpid(), strerror(errno));
            exit(1);
        }
        // ---------- Background first child: ----------------
        else if (strcmp(arglist[count-1], "&") == 0){
            signal(SIGINT, SIG_IGN); //ignoring SIGINT when in background child process
            *is_background = 1;
            arglist[count-1] = NULL; // removing the "&" sign from the arglist in order to pass it to execvp
            execvp(arglist[0], arglist); // <-- execvp
            // will get here only if the execvp failed:
            fprintf(stderr, "exec of child %d failed. %s\n",getpid(), strerror(errno));
            exit(1);
        }
        // ---------- foreground first child: ------------------
        else{
            execvp(arglist[0], arglist); // <-- execvp
            // will get here only if the execvp failed:
            fprintf(stderr, "exec of child %d failed. %s\n",getpid(), strerror(errno));
            exit(1);
        }
    }
    // ============================================== fork1 error: ======================================================
    else{
        fprintf(stderr, "fork failed\n.");
        return 0; // return 0 in case of an error (as asked in instructions).
    }
    return 1;
}


// ##############################################   finalize:   #######################################################
int finalize(void){
    return 0;
}


