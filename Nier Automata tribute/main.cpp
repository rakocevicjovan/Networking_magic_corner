#include "Relay.h"
#include "FSM.h"
#include "GUI.h"
#include "MatchMaker.h"
#include "HostingScreen.h"
#include "JoiningScreen.h"
#include "DefeatScreen.h"
#include "VictoryScreen.h"
#include "Game2B.h"
#include "Game9s.h"
#include <string>

#include "InputManager.h"

static const float HOST_LIST_UPDATE_INTERVAL = 3.0f;
float ww = 1920.f * 0.5f, wh = 1080.f * 0.5f, elapsed = 0.f, elapsedSinceHostUpdate = 0.f, late = 0.f, frameTime = 0.0f;
bool run = true;
bool g2bfirst = true, g9sfirst = true;

std::pair<std::string, int> joinIpPort;

MatchMaker matchMaker;
Relay relay;

GameState gameState = GameState::MAIN_MENU;

HostingScreen hScreen;
JoiningScreen jScreen;
VictoryScreen vScreen;
DefeatScreen  dScreen;

InputManager iMan;

sf::RenderWindow window(sf::VideoMode(ww, wh), "Nier but bad", sf::Style::Close | sf::Style::Titlebar);

int main() {

	//initiate some game systems
	window.setKeyRepeatEnabled(false);

	GUI gui(&window);
	gui.init();

	Game2B* g2bPtr = nullptr;
	Game9S* g9sPtr = nullptr;

	//network stuff
	matchMaker.setHost("127.0.0.1");
	relay.init();
	relay.tcpi.unblock();

	sf::Clock deltaClock;
	sf::Event e;
	
	//prepare loading screens
	sf::Texture texHosting, texJoining, texVictory, texDefeat;
	texHosting.loadFromFile("../Assets/loading_2b.jpg");
	texJoining.loadFromFile("../Assets/loading_9s.jpg");
	texVictory.loadFromFile("../Assets/Victory.jpg");
	texDefeat.loadFromFile("../Assets/Defeat.jpg");

	sf::Sprite picHosting, picJoining, picVictory, picDefeat;
	picHosting.setTexture(texHosting);
	picJoining.setTexture(texJoining);
	picVictory.setTexture(texVictory);
	picDefeat.setTexture(texDefeat);

	hScreen.setUp(picHosting, window, "Waiting for 9s...");
	jScreen.setUp(picJoining, window, "Searching for 2b...");
	vScreen.setUp(picVictory, window, "[FOR THE GLORY OF MANKIND]");
	dScreen.setUp(picDefeat,  window, "2B... It was an honor to fight with you. Truly.");

	//jam frame updates in here
	while (window.isOpen() && run) {

		//frame timing
		sf::Time dt = deltaClock.restart();
		frameTime = dt.asSeconds();
		elapsed += frameTime;
		late += frameTime;
		

		if (gameState == GameState::MAIN_MENU)
		{
			run = gui.react(window, e, gameState);
		}



		if (gameState == GameState::HOSTING) 
		{
			//loading screen while waiting for joins
			if (!matchMaker.hosting) {
				matchMaker.host();
				relay.tcpi.listen();
			}
			else {
				//listen for an incoming connection!
				if (relay.tcpi.accept()) {					
					relay.tcpi.closeListener();
					matchMaker.unhost();
					gameState = GameState::PLAYER_2B;
				}
				if (hScreen.update(window)) {
					gameState = GameState::MAIN_MENU;
					matchMaker.unhost();
				}
			}
		}



		if (gameState == GameState::JOINING) 
		{
			elapsedSinceHostUpdate += frameTime;
			if (elapsedSinceHostUpdate > HOST_LIST_UPDATE_INTERVAL) {
				elapsedSinceHostUpdate -= HOST_LIST_UPDATE_INTERVAL;
				jScreen.updateList(matchMaker.listGames());
			}

			joinIpPort = std::make_pair("-1", -1);
			if (jScreen.update(window, joinIpPort))
				gameState = GameState::MAIN_MENU;

			jScreen.drawList(window);

			//when accepted by the sever
			if (joinIpPort.second != -1) {
				relay.tcpi.block();
				relay.tcpi.connect(joinIpPort.first, joinIpPort.second);
				relay.tcpi.unblock();
				gameState = GameState::PLAYER_9S;
			}
			
		}



		if (gameState == GameState::OBSERVING) 
		{
			//ignore for now... implement if there is enough time...
			//receive updates from player one and render everything
		}



		if (gameState == GameState::PLAYER_2B)
		{

			if (g2bfirst) {
				g2bPtr = new Game2B(window);
				g2bPtr->init();
				relay.attachPlayerObserver(g2bPtr->player);
				g2bPtr->attachRelay(relay);
				g2bfirst = false;
				g2bPtr->first = true;
			}

			g2bPtr->update(frameTime, window, gameState);
		}



		if (gameState == GameState::PLAYER_9S)
		{

			if (g9sfirst) {
				g9sPtr = new Game9S(window);
				g9sPtr->init();
				relay.attachPlayerObserver(g9sPtr->player);
				g9sPtr->attachRelay(relay);
				g9sfirst = false;
				g9sPtr->first = true;
			}

			g9sPtr->update(frameTime, window, gameState);
		}



		if (gameState == GameState::VICTORY)
		{
			if (g2bPtr != nullptr || g9sPtr != nullptr)
			{
				relay.tcpi.disconnect();
				window.setView(window.getDefaultView());

				delete g2bPtr;
				delete g9sPtr;
				g2bPtr = nullptr;
				g9sPtr = nullptr;

				g2bfirst = true;
				g9sfirst = true;
			}

			if (vScreen.update(window))
				gameState = GameState::MAIN_MENU;
		}



		if (gameState == GameState::DEFEAT)
		{
			if (g2bPtr != nullptr || g9sPtr != nullptr) 
			{
				relay.tcpi.disconnect();
				window.setView(window.getDefaultView());

				delete g2bPtr;
				delete g9sPtr;
				g2bPtr = nullptr;
				g9sPtr = nullptr;

				g2bfirst = true;
				g9sfirst = true;
			}


			if (dScreen.update(window))
				gameState = GameState::MAIN_MENU;
		}

		window.display();
		window.clear();
	}

	matchMaker.unhost();
	return 0;
}