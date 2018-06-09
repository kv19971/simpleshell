#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/types.h>

#define MAX_CMDLINE_LEN 256
char CWD[1024];
/* function prototypes go here... */

void pstrcpy (const char * src, char * dest, int len){
    int i;
    for(i = 0; i < len; i++){
        dest[i] = src[i];
    }
    dest[len] = '\0';
}


void show_prompt();
int get_cmd_line(char *cmdline);
void process_cmd(char *cmdline);


/* The main function implementation */
int main()
{
	char cmdline[MAX_CMDLINE_LEN];
	
	while (1) 
	{
		show_prompt();
		if ( get_cmd_line(cmdline) == -1 )
			continue; /* empty line handling */
		
		process_cmd(cmdline);
	}
	return 0;
}

int getNumWords(const char * string){
    int len = strlen(string);
    int words = 0;
    int i;
    for(i = 0; i < len; i++){
        if(i == (len-1) || ((string[i] == ' ' || string[i] == '\t') && i < (len-1))){
            if(!((i==0) || ((string[i-1] == ' ' || string[i-1] == '\t'))) 
            || (i==(len-1) && (string[i] != ' ' && string[i] != '\t'))){
                // allwords[words] = malloc(sizeof(char)*(i-st+1));
                // pstrcpy(&string[st], allwords[words], (i-st));
                words++;
                
            }
        }  
    }
    return words;
}


char ** ssplit(char const * string){
    int words = getNumWords(string);
    int len = strlen(string);
    char ** allwords = malloc(sizeof(char*)*(words+1));
    int st = 0;
    int range = -1;
    words = 0;
    int i;
    for(i = 0; i < len; i++){
        if(i == (len-1) || ((string[i] == ' ' || string[i] == '\t') && i < (len-1))){
            if(!((i==0) || ((string[i-1] == ' ' || string[i-1] == '\t'))) 
            || (i==(len-1) && (string[i] != ' ' && string[i] != '\t'))){
            //if(){
                if(i == len-1 && (string[i] != ' ' && string[i] != '\t')){
                    range = i-st+1;
                }else{
                    range = i-st;
                }
                allwords[words] = malloc(sizeof(char)*(range+1));
                pstrcpy(&string[st], allwords[words], (range));
                words++;
                
            }
            st = i+1;
        }  
    }
    allwords[words] = NULL;
    return allwords;
}

int check_background(char ** args, int len){
    char last = args[len-1][0];
    if(last == '&'){
        free(args[len-1]);
        args[len-1] = NULL;
        return 1;
    }else{
        return 0;
    }
}

void builtin_chdir(char** path){
    int ret;
    if(path[0] == NULL){
        ret = chdir(getenv("HOME"));
    }else{
        ret = chdir(path[0]);
    }
    if(ret != 0){
        printf("INVALID DIRECTORY\n");
    }
}

void builtin_sleep(char** args, int bg){
    if(args[1] == NULL){
        printf("PLEASE SPECIFY TIME\n");
        return;
    }
    int secs = atoi(args[1]);
    pid_t pid = fork();

    if(pid>0){
        int stat;
        printf("CHILD PID: %i SLEEPING FOR %i SECONDS\n", pid, secs);
        
        if(bg == 1){
            printf("RUNNING PID %i IN BACKGROUND\n", pid);
        }else{
            waitpid(pid, &stat, 0);     
            printf("SLEEP DONE. PID: %i STATUS: %i\n", pid, stat);   
        }
    }else{
        sleep(secs);
        exit(0);
    }
}

void shell_start(const char * name, char ** args, int bg){
    pid_t cpid, wpid;
    int status;
    cpid = fork();
    if(cpid < 0){
        perror("ERROR RUNNING COMMAND\n");
    }else if(cpid == 0){
        if(execvp(name, args) == -1){
            perror("ERROR RUNNING PROCESS");
            exit(EXIT_FAILURE);
        }
        exit(0);
        
    }else{
        //waitpid(cpid, &status, 0);
        if(bg == 1){
            printf("RUNNING PID %i IN BACKGROUND\n", cpid);
        }else{
           /* do {
                wpid = waitpid(cpid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));*/
            waitpid(cpid, &status, 0);
        }
    }
}
void ssplit_cleanup(char ** list, int len){
    int i;
    for(i = 0; i < len; i++){
        if(list[i] != NULL){
            free(list[i]);
        }
    }
    free(list);
}

int get_chr(char *cmdline, char param){
    char * pipe = strchr(cmdline, param);
    if(pipe == NULL){
        return -1;
    }else{
        return (int)(pipe - cmdline);
    }
}

int get_redirin_chr(char *cmdline){
    return get_chr(cmdline, '<');
}

int get_redirout_chr(char *cmdline){
    return get_chr(cmdline, '>');
}

int get_pipe_chr(char *cmdline){
    return get_chr(cmdline, '|');
}

void process_cmd(char *cmdline)
{

    int possible_redirout = get_redirout_chr(cmdline);
    if(possible_redirout > -1){
        char * cmd = strtok(cmdline, ">");
        char * fin = strtok(NULL, " ");
        int fin_desc = creat(fin, S_IWUSR); 
        if(fin_desc < 0){
            printf("CANT WRITE TO FILE  \n");
        }
        pid_t godpid = fork();
        if(godpid == 0){
            
            close(STDOUT_FILENO);
            if(dup2(fin_desc, STDOUT_FILENO) < 0){
                printf("CANT DUPLICATE \n");
            }
            close(fin_desc);
            
            process_cmd(cmd);
            exit(0);
        }else{
            wait(0);
            return;
        }
    }

    // split string by space, first element is actual command, rest are args
    int possible_pipe = get_pipe_chr(cmdline);
    if(possible_pipe > -1){
        char * first_cmd = strtok(cmdline, "|");
        pid_t godpid = fork();
        if(godpid == 0){
            int pfds[2];
            pipe(pfds);
            pid_t pid = fork();
            if(pid == 0){
                close(1); /* close stdout */
                dup(pfds[1]);   /* make stdout as pipe input*/
                close(pfds[0]); /* don't need this */
                process_cmd(first_cmd);
                exit(0);
            }else if(pid > 0){
                close(0); /* close stdin */
                dup(pfds[0]);  /* make stdin as pipe output*/
                close(pfds[1]); /* don't need this */
                wait(0); /* wait for the child process */
                process_cmd(&cmdline[possible_pipe + 1]);
                
                
            }else{
                printf("LAUNCH FAILED.\n");
            }
            exit(0);
        }else{
            wait(0);
            return;
        }
    } 

    int possible_redirin = get_redirin_chr(cmdline);
    if(possible_redirin > -1){
        char * cmd = strtok(cmdline, "<");
        char * fin = strtok(NULL, " ");
        
        int fin_desc = open(fin, O_RDONLY); 
        if(fin_desc < 0){
            printf("CANT OPEN FILE  \n");
        }
        pid_t godpid = fork();
        if(godpid == 0){
            
            close(STDIN_FILENO);
            if(dup2(fin_desc, STDIN_FILENO) < 0){
                printf("CANT DUPLICATE \n");
            }
            close(fin_desc);
            process_cmd(cmd);
            
            exit(0);
        }else{
            wait(0);
            
        }
        
        return;
    }
    
    
    

    char ** cmdargs = ssplit(cmdline);
    char * proc_name = cmdargs[0];
    char ** restargs = &cmdargs[1];
    // int i;
    // printf("ALL %i ARGS: ", getNumWords(cmdline));
    // for(i=0; i < getNumWords(cmdline); i++){
    //     printf("%s|", cmdargs[i]);
    // }
	if(strcmp(proc_name, "exit") != 0){ //if command not exit then go ahead
        int bg = check_background(cmdargs, getNumWords(cmdline));
        if(strcmp(proc_name, "child") == 0){
            builtin_sleep(cmdargs, bg);
        }else if(strcmp(proc_name, "cd") == 0){
            builtin_chdir(restargs);
        }else{
            shell_start(proc_name, cmdargs, bg);
        }
        ssplit_cleanup(cmdargs, getNumWords(cmdline)+1);
    }else{ //else exit shell
        ssplit_cleanup(cmdargs, 2);
        exit(0);
    }
}



void show_prompt() 
{
    if (getcwd(CWD, sizeof(CWD)) != NULL){
        char * last = strrchr(CWD, '/') + 1;
        
        printf("[%s] ", last);
    }
	printf("myshell> ");
}

int get_cmd_line(char *cmdline) 
{
    int i;
    int n;
    if (!fgets(cmdline, MAX_CMDLINE_LEN, stdin))
        return -1;
    // Ignore the newline character
    n = strlen(cmdline);
    cmdline[--n] = '\0';
    i = 0;
    while (i < n && cmdline[i] == ' ')
    {
        ++i;
    }
    if (i == n)
    {
        // Empty command
        return -1;
    }
    return 0;
}
