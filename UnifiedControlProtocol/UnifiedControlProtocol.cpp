// Consistent protocol for RS232 & UDP integration
//
// This script emulate the Butler XT2 RS232 protocol specification in Programmer. 
// It also provides the same protocol specification to be used over UDP,
// allowing for consistent system integration procdures regardless of the technology
// being used. 
//
// In addition to emulating the existing RS232 protocol from the Butler XT2, this script
// also adds a number of features, such as playing a specific cue within a cuelist,
// which are not supported by the XT2 in standalone mode. 
//


int sharedBob = BobAllocate(500);
string message;

if(undefined(_true)){
	int _false = 0;
	int _true = 1;
}

// Tracks whether or not we've successfully processed an incoming command
int commandProcessed = _false;

// Set up background functions to handle RS232 & UDP input
RegisterEvent(UdpReceive, OnUdpReceive);
RegisterEvent(SerialPort, OnSerialPort);
Suspend();

// Receive & process UDP input
function OnUdpReceive(int  nDriverHandle)
{	
	commandProcessed = _false;
	
	int drv = nDriverHandle;
	BobSetRange(sharedBob, 0, 500, 0);
	string ip = "";
	int port = -1;
	
	ReceiveFrom(drv, sharedBob, ip, port);
	message = BobGetString(sharedBob, 0, 500);
	routeMessage();
}


// Receive & pass the data to the processing function
function OnSerialPort(int dummy1, int dummy2, int dummy3) 
{
	processSerialCommand();
}

// Process a command coming from the serial port
function processSerialCommand(){
	commandProcessed = _false;
	int drv = DriverGetHandle("serial");
	int len;
	message = GetSerialString(drv,len);
	
	// Process the command if there are enough characters
	if(len>2){
		routeMessage();	
	} 
	
	// Run this function again unless we've cleared the buffer
	if(len>-1){
		processSerialCommand();
	}
}


// Look at the first two characters do determine what command is being
// requested & run the relavent function. Print an error to the log
// window if the incoming message isn't recognized as a valid command
function routeMessage(){
	message = strToUpper(message);
	
	// Verify that we have enough characters to process a command
	checkMinMessageLength(2);
	
	string command = midstr(message, 0, 2);
	
	if(strcmp(command, "PC") ==0) {cmdPlayCuelist();}		
	if(strcmp(command, "PQ") ==0) {cmdPlayCue();}			
	if(strcmp(command, "IN") ==0) {cmdSetVmLevel();}
	if(strcmp(command, "ST") ==0) {cmdStopCuelist();}
	if(strcmp(command, "TP") ==0) {cmdTogglePlay();}
	if(strcmp(command, "PP") ==0) {cmdTogglePause();}
	if(strcmp(command, "AF") ==0) {cmdAutoFade();}
	if(strcmp(command, "NX") ==0) {cmdPlayNextMutex();}
	if(strcmp(command, "PX") ==0) {cmdPlayPrevMutex();}
	
	if(!commandProcessed){
		alert("UDP Command was not recognized.\n");
	}
}


// Plays the specified cuelist
// cmd="PC"
// Param1 - Cuelist number
function cmdPlayCuelist(){
	int cuelist = QL(getParam(0));
	CuelistStart(cuelist);
	commandProcessed = _true;
}

// Plays the specified within a cuelist
// cmd="PQ"
// Param1 - Cuelist number
// Param2 - Cue number
function cmdPlayCue(){
	int cuelist = QL(getParam(0));
	int cue = Q(getParam(1));
	CuelistGotoCue(cuelist, cue, _fade);
	commandProcessed = _true;
}

// Set the level of a versatile master
// cmd="IN"
// Param1 - VM Number
// Param2 - Level (0..100)
function cmdSetVmLevel(){
	int vm = getParam(0);
	int level = getParam(1);
	VersatileMasterStartAutoFade(vm, level, 0);
	commandProcessed = _true;
}

// Fades a VM in the specified time
// cmd="AF"
// Param1 - VM Number
// Param2 - Level (0..100)
// Param3 - Fade time in seconds
function cmdAutoFade(){
	int vm = getParam(0);
	int level = getParam(1);
	int time = 1000* getParam(2);
	VersatileMasterStartAutoFade(vm, level, time);
	commandProcessed = _true;
}

// Stop the specified cuelist
// cmd="ST"
// Param1 - Cuelist number
function cmdStopCuelist(){
	int cuelist = QL(getParam(0));
	if(cuelist==QL(0)){
		CuelistStopAll();
	} 
	else {
		CuelistStop(cuelist);
	}
	commandProcessed = _true;
}

// Toggle play on the spcified cuelist
// cmd="TP"
// Param1 - Cuelist number
function cmdTogglePlay(){
	int cuelist = QL(getParam(0));
	int currentState = CueGetCurrent(cuelist);
	
	if(currentState<0){
		CuelistStart(cuelist);
	} 
	else {
		CuelistStop(cuelist);
	}
	commandProcessed = _true;
}

// Play next cuelist in a mutex group
// cmd="NX"
// Param1 - Mutex Group
function cmdPlayNextMutex(){
	int mutex = getParam(0);
	playNextMutex(mutex);

	int ql = getNextMutexCuelist(mutex);
	CuelistStart(ql);

	commandProcessed = _true;
}

// Play previous cuelist in a mutex group
// cmd="PX"
// Param1 - Mutex Group
function cmdPlayPrevMutex(){
	int mutex = getParam(0);
	playPrevMutex(mutex);

	int ql = getPrevMutexCuelist(mutex);
	CuelistStart(ql);

	commandProcessed = _true;
}

// Toggle pause on specified cuelist
// cmd="PP"
// Param1 - Cuelist
function cmdTogglePause(){
	int cuelist = QL(getParam(0));
	int currentStatus = CuelistIsPaused(cuelist);
	
	CuelistPause(cuelist);
	
	
	if(currentStatus == _true){
		CuelistStart(cuelist);
	} 
	else {
		CuelistPause(cuelist);
	}
	
	commandProcessed = _true;
}

// Get the next cuelist in a mutex group, wrapping around if neccessary. 
function getNextMutexCuelist(int mutex){
	int i;
	int qlMutex;
	
	int cuelist = CuelistMutexGetStatus(mutex);
	
	for (i=cuelist+1; i<=100; i++)
	{
		if(cuelistExists(i)){
			
			qlMutex = CuelistGetProperty(i, "MutualExcludeGroup");
			if(mutex == qlMutex){
				return i;
			}
		}
	}	
	
	for (i=0; i<=cuelist-1; i++)
	{
	
		if(cuelistExists(i)){
		
			qlMutex = CuelistGetProperty(i, "MutualExcludeGroup");
			if(mutex == qlMutex){
				return i;
			}
		}
	}
	
	return -1;
}

// Get the previous cuelist in a mutex group, wrapping around if neccessary. 
function getPrevMutexCuelist(int mutex){
	int i;
	int qlMutex;
	
	int cuelist = CuelistMutexGetStatus(mutex);
	
	for (i=cuelist-1; i>=0; i--)
	{
		if(cuelistExists(i)){
			
			qlMutex = CuelistGetProperty(i, "MutualExcludeGroup");
			if(mutex == qlMutex){
				return i;
			}
		}
	}	
	
	for (i=100; i>=cuelist+1; i--)
	{
	
		if(cuelistExists(i)){
		
			qlMutex = CuelistGetProperty(i, "MutualExcludeGroup");
			if(mutex == qlMutex){
				return i;
			}
		}
	}
	
	return -1;
}


// Returns true if the spcified cuelist contains at least 1 cue
function cuelistExists(int cuelist){
	return (CueGetCount(cuelist)>0);
}


// Parse the 3 digit parameters from the command string
function getParam(int paramNum){
	int start = 2 + (paramNum * 3);
	int count = 3;
	
	int minMessageLength = start + count;
	checkMinMessageLength(minMessageLength);
	
	string paramStr = midstr(message, start, count);
	return val(paramStr);
}

// Return true if the message variable's length is at least the specified value. 
function checkMinMessageLength(int length){
	if(strlen(message)<length){
		alert("Invalid UDP Message received: ERR_LENGTH %d, %d\n", strlen(message), length);
		bailOut();
	}
}

// Exit out and restart the script if we run into an error. This will reset everything
// while making sure that the script is still running in the background. 
function bailOut(){
	Call("unifiedControlProtocol", 0, 0, 0);
	exit();
}

 