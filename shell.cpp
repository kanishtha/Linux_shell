#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<iostream>
#include<string>
#include<vector>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/param.h>
#include<fcntl.h>
#include<fstream>
#include<cstring>
#include<map>
#include<iostream>
#include <sys/wait.h>
#include<iterator>
#include <signal.h>
#define HISTSIZE 20
#define SIZE 100

using namespace std;

/************************************************************ Function Prototypes *******************************************************/


void getcur_working_dir();
void change_directory(string path);
void handle_bangs(string line);
void environment_var(string env_var);
void echo_implementation(string arg[],int numarg);
void display_history();
void history_implementation(string arg[],int numarg);
void storing_individual_cmds(string line);
void parser(string line);
void handling_cmds(string cmd,int index);


/************************************************************ Global Variables **********************************************************/
int pipe_indexes[1000];
int k;
string individual_cmds[1000];
int no_of_cmds;
vector<string> hist;
ifstream myfile;
char chd[1024];
map<string,string> exp;
int input_re_index;
int output_re_index;
int bg_index;
string all_cmds[20][20];
int nopt[20];
string input_fn;
string output_fn;
int pipes[100][2];
int ifd;
int ofd;
pid_t pid;
pid_t cpid;
int expfd;

/************************************************************ Signal Handling ************************************************************/
void sigint(int a)
{
	cout<<endl<<"Prompt:";
	cout.flush();
}

/************************************************************* Clear Data *****************************************************************/

void clear_data(){
memset(nopt,0, sizeof(nopt) * 20);
memset(pipes,0, sizeof(pipes[0][0]) * 100 * 2);
//memset(individual_cmds, "", sizeof(individual_cmds[0][0]) * 1000);
int i;
for(i=0;i<1000;i++)
	individual_cmds[i]='\0';
int j;
for(i=0;i<20;i++)
	for(j=0;j<20;j++)
		all_cmds[i][j]='\0';
input_fn='\0';
output_fn='\0';
no_of_cmds=0;
}

/********************************************************** Close all pipes ***********************************************************/
void close_all_pipes(){
int i;
for(i=0;i<no_of_cmds-1;i++){
	close(pipes[i][0]);
	close(pipes[i][1]);
}
}

/*************************************************** Check whether a command is shell builtin or not **********************************/

int check_builtin(string ar){

	if(ar.compare("echo")==0){
		return 1;
	}
	if(ar.compare("history")==0){
		return 2;
	}
	if(ar.compare("pwd")==0){
		return 4;	
	}
	if(ar[0]=='!'){
		return 5;
	}

}

void check_redirections(string ar[],int size){
int i;
input_re_index=0;
output_re_index=0;
for(i=0;i<size;i++){
	if(ar[i].compare("<")==0){
		input_re_index=i;
		input_fn = ar[i+1];
	}
/*	if(ar[i][0]=='<'){
		ar[i]=ar[i].substr(1,ar[i].length()-1)
		input_fn=ar[i];
	}
*/
	if(ar[i].compare(">")==0){
		output_re_index=i;
		output_fn = ar[i+1];
	}
/*	if(ar[i][0]=='>'){
		ar[i]=ar[i].substr(1,ar[i].length()-1)
		output_fn=ar[i];
	}
*/
}
}

/*************************************************** Execute command through execvp() **********************************************/

void execute_cmd(int i){
int set=0;
int status;
char *argv[nopt[i]+1];
int z;
check_redirections(all_cmds[i],nopt[i]);
int z1=0;
for(z=0;z<nopt[i];z++){
	if(input_re_index != 0 && z == input_re_index){
		ifd = open(input_fn.c_str(), O_RDONLY,0777);
		continue;	
	}
	if(output_re_index != 0 && z == output_re_index){
		ofd = open(output_fn.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR,0777);
		z++;
		continue;	
	}
	
	argv[z1]=(char*)all_cmds[i][z].c_str();
		z1++;
}
argv[z1]='\0';

if((pid=fork())==0){
cpid=getpid();
	if(i==0){
		if(input_re_index!=0){
			close(0);
			dup2(ifd,0);
		}
		if(output_re_index != 0){
			close(1);
			dup2(ofd,1);
		}
		else if(no_of_cmds > 1){
		close(1);
		dup2(pipes[0][1],1);
		}
	}
	else if(i==no_of_cmds-1){
		if(output_re_index !=0){
			close(1);
			dup2(ofd,1);
		}
		close(0);	
		dup2(pipes[i-1][0],0);
	}
	else{
		close(0);
		dup2(pipes[i-1][0],0);
		close(1);
		dup2(pipes[i][1],1);
	}
if(check_builtin(all_cmds[i][0])==1){ //echo
	echo_implementation(all_cmds[i],nopt[i]);
	set=1;
}
else if(check_builtin(all_cmds[i][0])==2){ //history
	if(nopt[i]==1)
		display_history();
	if(nopt[i]==2)
		history_implementation(all_cmds[i],nopt[i]);
	set=1;
}
else if(check_builtin(all_cmds[i][0])==4){  //pwd
	getcur_working_dir();
	set=1;
}
else if(check_builtin(all_cmds[i][0])==5){	//!
handle_bangs(all_cmds[i][0]);
set=1;
}
else {
execvp(all_cmds[i][0].c_str(),argv);
	perror("No Command Found");
	exit(EXIT_FAILURE);
}
}
else{
while(waitpid( pid, &status, 0 ) != pid);
//if( WIFEXITED( status )==0 && WEXITSTATUS( status ) != 0  && set==0)
//	cout<<"No Command Found"<<endl;
close(pipes[i][1]);
if(i>0)
close(pipes[i-1][0]);
if(input_re_index != 0)
close(ifd);
if(output_re_index != 0)
close(ofd);
}
}


/**************************************************** Implement Pipes *************************************************************/
void implement_pipes(){

	int i;
	for(i=0;i<no_of_cmds-1;i++){
		if(pipe(pipes[i])==-1)
			cout<<"Error in Pipe Creation"<<"\n";
	}
	for(i=0;i<no_of_cmds;i++)
		execute_cmd(i);
}

/***************************************************** Execute *********************************************************************/

void  execute()
{
     pid_t  pid1;
     int status;
	if ((pid1 = fork()) < 0) {    
          printf("Child process cannot be created\n");
     }
     else if (pid1 == 0) {          
               implement_pipes();
		exit(0);
     }
     else {                        
          while (wait(&status) != pid1);    
               
     }
}



/********************************************* Handling assignments *********************************************************************/

//void handle_assignments(string var,string val){
//m[var]=val;
//}

/**************************************************** Display Current Working Directory ************************************************/
void getcur_working_dir(){
char cwd[1024];
if(getcwd(cwd, sizeof(cwd)) != NULL)
       cout<<cwd<<"\n";
}

/*************************************************** change Directory *****************************************************************/
void change_directory(string path){
//cout<<"change_directory";
int rc;
if(path.compare("--")==0 || path.compare("#~")==0 || path[0]=='#' || path.compare("/~")==0 || path.compare("~")==0){
path="HOME";
chdir(getenv(path.c_str()));
}
else if(path.compare("-")==0){
	if(chd)
	chdir(chd);
}
else if(path.compare("~#")==0)
cout<<"cd: ~#: No such file or directory\n";
else{
//cout<<"final else";
//cout<<path;
rc=chdir(path.c_str());
			if(rc<0)
				perror("chdir() error");
}
}

/*********************************************** Write to History.txt ******************************************************************/

void write_to_file(){
	//char *p = getenv("HOME");
	//string str(p);
	//string fn = str + "/" + "History.txt";
	ofstream ofs("History.txt");
	int i;
	for(i=0;i<hist.size();i++)    
     		ofs << hist[i] << endl;
	ofs.close();

}

/*********************************************** Load the contents of History.txt in a Vector ******************************************/
void load_vector(){
	//char *p = getenv("HOME");
	//string str(p);
	//string fn = str + "/" + "History.txt";
	ifstream myfile("History.txt");
	string fline;
	while (getline(myfile,fline))
                hist.push_back(fline);
}


/**************************************************** Handle Bangs *******************************************************************/

void handle_bangs(string line){
	string cmd;
	string last = hist[hist.size()-1];
	if(last.compare(line) != 0 && line[0] != '!'){
		hist.push_back(line);
	}
	else{
		if(line[0]=='!' && line[1]=='!'){
			cmd = hist[hist.size()-1]; 			
			cout<<cmd<<"\n";
		}
		else if(line[0]=='!' && line[1]=='-'){
			string temp = line.substr(2,strlen(line.c_str())-2 );
			int index = stoi(temp);
			cmd = hist[hist.size()-index];
			cout<<cmd<<"\n";
			if(index >1)
				hist.push_back(cmd);
		}
		else if(line[0]=='!' && line[1]!='-'){
			int strt=0;
			if(hist.size() > HISTSIZE)
				strt=hist.size()- HISTSIZE;
			string temp = line.substr(1,strlen(line.c_str())-1 );
                        int index = stoi(temp);
			cmd = hist[strt+index-1];
			cout<<cmd<<"\n";
			if((strt+index-1) != hist.size()-1)
				hist.push_back(cmd);
		}
	}
		if(cmd.length()!=0){
			if(cmd[0] != '!')
				hist.push_back(cmd);
			parser(cmd);
			int m;
			for(m=0;m<no_of_cmds;m++){
				handling_cmds(individual_cmds[m],m);
			}
			if(all_cmds[no_of_cmds-1][0].compare("cd")==0){
				change_directory(all_cmds[no_of_cmds-1][1]);	
			}
			else{	
			execute();
			}
			
		}
				
	
}

/********************************************** echoing Environment Variables *********************************************************/

void environment_var(string env_var){
string env = env_var.substr(1,strlen(env_var.c_str()));
char *p = getenv(env.c_str());
if(p!= NULL)
	cout<<getenv(env.c_str())<<" ";
else{
cout<<exp[env]<<" ";
}
}

/********************************************************* Handle Echo Args **************************************************************/

void handle_echo_args(string arg){
	int i;
	string marg="";
	if(arg[0]=='\"'){
		for(i=0;arg[i] != '\0';i++){
			if(arg[i]=='\"')
				continue;
			else
			marg=marg+arg[i];
		}
		cout<<marg<<" ";
		return;
	}
	else if(arg[0]=='\''){
		for(i=0;arg[i] != '\0';i++){
			if(arg[i]=='\'')
				continue;
			else
			marg=marg+arg[i];
		}
		cout<<marg<<" ";
		return;
	}
	else if(arg[0] != '\'' && arg[0] != '\"'){
		cout<<arg<<" ";
		return;
	}

}

/**********************************************************  Echo Implementation *********************************************************/

void echo_implementation(string arg[],int numarg){
int i;

for(i=1;i<numarg;i++){
	if(arg[i][0]!='$'){
		handle_echo_args(arg[i]);
	}
	else if(arg[i].compare("$$")==0){
		cout<<cpid;
	}
	else if(arg[i][0]=='$' && arg[i][1]!='$'){
		environment_var(arg[i]);
	}
}
cout<<"\n";

}

/*************************************************** Display History *******************************************************************/

void display_history(){
int strt=0;
int it;
if(hist.size() > HISTSIZE)
	strt=hist.size()-HISTSIZE;
for(it=strt;it<hist.size();it++)
	cout<<hist[it]<<"\n";
}

/******************************************************* History Implementation **********************************************************/
void history_implementation(string arg[],int numarg){
int i;
if(numarg==2){
int strt = hist.size() - stoi(arg[1]);
for(i=strt;i<hist.size();i++)
	cout<<hist[i]<<"\n";
}
}



/********************************************* Handling Individual Commands and their Parsing  *******************************************/

void handling_cmds(string cmd,int index){

int i=0;
        while(cmd[i] != '\0'){
	        if(cmd[i] != ' ')
		        break;
	        i++;
        }
cmd = cmd.substr(i,strlen(cmd.c_str())-i);
i=0;
int m;
int n;
int strtsq=0;
int endsq=0;
int strtdq=0;
int enddq=0;
int space_indexes[1000];
string arg[100];
for(m=0;m<1000;m++)
        space_indexes[m]=0;
	m=0;
	while(cmd[i] != '\0'){
		if(cmd[i]=='\'' && strtsq==0 && strtdq==0)
			strtsq=1;
		else if(cmd[i]=='\"' && strtdq==0 && strtsq==0)
			strtdq=1;
		else if(cmd[i]=='\'' && strtsq==1){
			endsq=1;
			strtsq=0;
		}
		else if(cmd[i]=='\"' && strtdq==1){
			enddq=1;
			strtdq=0;
		}
		else if(cmd[i] == ' ' && strtsq == 0 && strtdq == 0){
			space_indexes[m]=i;
			m++;
		}
		i++;
	}
	
int prev = space_indexes[0];
for(n=1;n<m-1;n++){
	if(space_indexes[n] == prev+1){
		prev=space_indexes[n];
		space_indexes[n]=-1;
	}
	else{
		space_indexes[n-1]=prev;
		prev=space_indexes[n];
	}
}
	if(m==0){
		all_cmds[index][0]=cmd.substr(0,cmd.length());
		nopt[index]=1;
	}
	else{
	all_cmds[index][0]=cmd.substr(0,space_indexes[0]);
	int t=1;
	for(n=1;n<m;n++){
		if(space_indexes[n]!=-1 && space_indexes[n-1] != -1){
			all_cmds[index][t]=cmd.substr(space_indexes[n-1]+1,space_indexes[n]-space_indexes[n-1]-1);
			t++;
		}
	}
	all_cmds[index][t]=cmd.substr(space_indexes[m-1]+1,strlen(cmd.c_str())-space_indexes[m-1]);
	t++;
	if(index != no_of_cmds-1)
		t=t-1;	
	nopt[index]=t;
	}
}



/****************************************************** Storing Individual Commands ******************************************************/
void storing_individual_cmds(string line){
	int j;
	if(pipe_indexes[0]==0)
		individual_cmds[0]=line;
	else{
		individual_cmds[0]=line.substr(0,pipe_indexes[0]);
	}
	for(j=0;j<k-1;j++){
		individual_cmds[j+1]=line.substr(pipe_indexes[j]+1,pipe_indexes[j+1]-pipe_indexes[j]-1);
	}
	if(pipe_indexes[0] !=0)
	individual_cmds[++j]=line.substr(pipe_indexes[j]+1,strlen(line.c_str()) - pipe_indexes[j]-1);
	no_of_cmds=j+1;
}


/************************************************************ Parsing command *************************************************************/
void parser(string line){
	int i=0;
 	k=0;
	int strtsq=0;
	int endsq=0;
	int strtdq=0;
	int enddq=0;
	while(line[i] != '\0'){
		if(line[i]=='\'' && strtsq==0 && strtdq==0)
			strtsq=1;
		else if(line[i]=='\"' && strtdq==0 && strtsq==0)
			strtdq=1;
		else if(line[i]=='\'' && strtsq==1){
			endsq=1;
			strtsq=0;
		}
		else if(line[i]=='\"' && strtdq==1){
			enddq=1;
			strtdq=0;
		}
		else if(line[i] == '|' && strtsq == 0 && strtdq == 0){
			pipe_indexes[k]=i;
			k++;
		}
		i++;
	}
storing_individual_cmds(line);
}

/*********************************************************** Handle Export ****************************************************************/

void handle_export(string arg){
string var;
string val;
int i=0;

	while(arg[i] != '\0'){
		if(arg[i]=='=')
			break;
		i++;
	}

var = arg.substr(0,i);
val = arg.substr(i+1,arg.length()-i);
//cout<<var<<"\n";
//cout<<val<<"\n";
exp[var]=val;
cout<<"export="<<exp[var]<<"\n";
}



/************************************************************ Main ************************************************************************/
int main()
{	
	load_vector();
	int m=0;
	for(m=0;m<1000;m++)
		pipe_indexes[m]=0;
	system("clear");
	char cwd[1000];
	int i,j;
	while(1){
		signal(SIGINT, sigint);
		cout<<"\033[1;32mKSHELL@Kanishtha:~\033[0m";
		//cout<<"\033[1;31m"<<$getcwd(cwd,sizeof(cwd))<<"\033[0m";
		printf("%s$ ",getcwd(cwd, sizeof(cwd)));
		fflush(stdin);
		string cmd_entered;
		cmd_entered.clear();
        	getline(cin,cmd_entered);
		if(cmd_entered.compare("exit")==0){
			write_to_file();
			break;
		}
		else if(cmd_entered.length()!=0){
			if(cmd_entered[0] != '!')
				hist.push_back(cmd_entered);
			parser(cmd_entered);
			for(m=0;m<no_of_cmds;m++){
				handling_cmds(individual_cmds[m],m);
			}
			if(all_cmds[no_of_cmds-1][0].compare("cd")==0){
				change_directory(all_cmds[no_of_cmds-1][1]);	
			}
			else if(all_cmds[no_of_cmds-1][0].compare("export")==0){
				if(nopt[0]>2)
					cout<<"command not found\n";
				else{
				expfd=open("Export.txt", O_WRONLY| O_CREAT|O_APPEND,0777);
				//const void *st=all_cmds[0][1].c_str();
				//write(expfd,st,sizeof(st));
				handle_export(all_cmds[0][1]);
				}
			}
			else{	
			execute();
			}
			
		}
		else{
		//	clear_data();
			continue;
		}
	}
	return 0;
}
	
