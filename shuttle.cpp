// Simulation of::: the Hertz airport shuttle bus, which picks up passengers
// from the airport terminal building going to the Hertz rental car lot.

#include <iostream>
#include "cpp.h"
#include <string.h>
//clude <ctime>    // For time()
#include <cstdlib>  // For srand() and rand(
//#include <memory>
//#include <memory>
#include <vector>
#include <sstream>
#include <queue>
using namespace std;

#define NUM_SEATS 20      // number of seats available on shuttle
#define TINY 1.e-20      // a very small time period
#define TERMNL2 2        // named constants for labelling event set
#define TERMNL1 1
#define CARLOT 0
mailbox whichS("which terminal");
facility_set *buttons;   // customer queues at each stop
facility_set *rest;           // dummy facility indicating an idle shuttle
facility_set *pickup;
facility_set *dropoff;
facility phone("to tell which shuttle phone");
event get_off_now ("get_off_now");  // all customers can get off shuttle
int T;
facility update;
queue<long> breakOver;
event_set* hop_on;  // invite one customer to board at this stop
event_set* get_off;
event boarded ("boarded");             // one customer responds after taking a seat
event_set * Terminals;
event_set* shuttle_called; // call buttons at each location
//long seats_used=0;
void make_passengers(long whereami);       // passenger generator
vector<string>places;
//[3] = {"Parking lot","Terminal 1","Terminal 2" }; // where to generate
long group_size();
int M;
void passenger(long whoami,long whereami);                // passenger trajectory
string people[3] = {"arr_cust", "third","dep_cust"}; // who was generated
vector<string> s;
 void shuttle(int,int );                  // trajectory of the shuttle bus consists of...
void loop_around_airport( long&,long);      // ... repeated trips around airport
void load_shuttle(long whereami, long & on_board,long); // posssibly loading passengers
qtable shuttle_occ("bus occupancy");  // time average of how full is the bus

extern "C" void sim(int argc, char *argv[])      // main process
{
  //converting to int
  T = atoi(argv[1]);
  int S = atoi(argv[2]);
  M = atoi(argv[3]);

  shuttle_called = new event_set("called shuttle" , T+1);
  buttons = new facility_set("curve", T+1);
  Terminals = new event_set("Terminals", (T+1));
  hop_on= new event_set("board shuttle", T+1);
  get_off = new event_set("get off", (T+1)*S);
  pickup =  new facility_set("pickup locations", T+1 );
  dropoff = new facility_set("drop off locations", T+1);
  rest = new facility_set("rest", S);
  places.push_back("parking lot");
  for(int i = 1; i <T+1; i++){
 	  places.push_back("Terminal " + to_string(i)  );
  }
	create("sim");
  shuttle_occ.add_histogram(NUM_SEATS+1,0,NUM_SEATS);

  for(long i = 0; i < T+1; i++){
    make_passengers(i);  // generate a stream of arriving customers
  }
  for(int i = 0; i < S;i++){
    shuttle(T,i);                // create a single shuttle
    }
  hold (1440);              // wait for a whole day (in minutes) to pass
  report();
  status_facilities();
}
// Model segment 1: generate groups of new passengers at specified location
void make_passengers(long whereami)
{
  const char* myName=places.at(whereami).c_str(); // hack because CSIM wants a char*
  create(myName);
  while(clock < 1440.)          // run for one day (in minutes)
  {
    hold(expntl(M));           // exponential interarrivals, mean 10 minutes
    long group = group_size();
    for (long i=0;i<group;i++)  // create each member of the group
    passenger(i,whereami);      // new passenger appears at this location
  }
}

// Model segment 2: activities followed by an individual passenger
void passenger(long whoami,long whereami)
{

  const char* myName=places.at(whereami).c_str(); // hack because CSIM wants a char*
  create(myName);
  long shuttle;
  (*buttons)[whereami].reserve();     // join the queue at my starting location
  (*shuttle_called)[whereami].set();  // head of queue, so call shuttle
  (*hop_on)[whereami].queue();        // wait for shuttle and invitation to board
  (*shuttle_called)[whereami].clear();// cancel my call; next in line will push
  hold(uniform(0.5,1.0));        // takes time to get seated
  boarded.set();                 // tell driver you are in your seat
  whichS.receive(&shuttle); // sendint which bus your in
  long *loc;
  //generate which terminal your going
  if(whereami == 0){
 	  loc = new long((rand()%T)+1) ;
  }else {
	   loc = new long(0);
  }
  (*buttons)[whereami].release();     // let next person (~:wif any) access button
  (*Terminals)[*loc ].clear();        // clear terminal you're in
  (*Terminals)[*loc  ].wait();            // everybody off when shuttle reaches next stop
  (*get_off)[*loc + (shuttle*(T+1))].clear();
  (*get_off)[*loc + (shuttle*(T+1))].wait(); // waiting for the shuttle to get to the terminal that you're going to
}
 // if any buttons in any terminal are press.
bool didOCC(int tn){
	for(int i=0;i < tn+1; i++){
		if((*shuttle_called)[i].state() == OCC) return true;
	}
	return false;
}


// Model segment 3: the shuttle bus
void shuttle(int tn,int snum) {
  string sn = to_string(snum);
  sn = "shuttle " + sn;
  const char* myName = sn.c_str();
  create ( myName);
  long  seats_used = 0;
  while(1) {  // loop forever
    // start off in idle state, waiting for the first call...
  long i =0;
  // which parking lot is empty ?
  while((*rest)[i].status() ==  BUSY){
    i++;
  }
  (*rest)[i].reserve(); // reserve a parking lot in the lot
  breakOver.push(i);

  shuttle_occ.note_value(seats_used);
  long whopush= (*shuttle_called).wait_any(); // loop exit needs to see event
   (*shuttle_called)[whopush].set(); // someone called the button let them get in line
   long j = breakOver.front(); // the one that has been in the parking longnest is set free
   breakOver.pop();     // pop it form FILA
   (*rest)[j].release(); // leaving the parking lot
    hold(2);  // 2 minutes to reach car lot stop
    // Keep going around the loop until there are no calls waiting
    while ( didOCC(tn) )
  	    loop_around_airport(seats_used,snum);
  }
}

long group_size() {  // calculates the number of passengers in a group
  double x = prob();
  if (x < 0.3) return 1;
  else {
  if (x < 0.7) return 2;
    else return 5;
  }
}

void loop_around_airport(long & seats_used, long S ) { // one trip around the airport
  // Start by picking up departing passengers at car lot
  // go to every terminal
  for (long i = 0; i < T+1; i++){
    hold (uniform(3,5));  // drive to airport terminal
    (*dropoff)[i].reserve();        // reserve the pick up off only shuttle in here
     int numGettingOff = (*get_off)[i + (S* (T+1)) ].wait_cnt(); // number of people waiting to get pick up
     (*get_off)[i + (S* (T+1)) ].set(); // let get off
     (*Terminals)[i].set();   // reserve which terminal you are in
     seats_used =  seats_used - numGettingOff; // subtract number of people that got off
     shuttle_occ.note_value(seats_used); // note the number of people that got off
    (*pickup)[i].reserve();              // reserve so that only one shuttle can get pick up.
    (*dropoff)[i].release();              // let them get in
   // how to keep track of the poeple in bus.  ?
     hold(1);
    load_shuttle(i, seats_used,S);        // load the number of people.
    shuttle_occ.note_value(seats_used);
    (*pickup)[i].release();
  }

}

void load_shuttle(long whereami, long & on_board, long S)  // manage passenger loading
{
  // invite passengers to enter, one at a time, until all seats are full
  while((on_board < NUM_SEATS) &&
    ((*buttons)[whereami].num_busy() + (*buttons)[whereami].qlength() > 0))
  {
    (*hop_on)[whereami].set();// invite one person to board
    boarded.clear();
    whichS.send(S);
    boarded.wait();  // pause until that person is seated
    on_board++;
    hold(TINY);  // let next passenger (if any) reset the button
  }
}
