#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/signal.h>
#include <sys/mman.h>
#include <string.h>
#include "util.h"
#include "serveur.h"
#include "fon.h"

static lSocket *globalSocketList;

void serverTCP (char *port) {
	clearScreen();
	printf("[serverTCP] Running chat as server on port: %s\n", port);

	int listeningSocket = createListeningSocket(port);
	globalSocketList = mmap(NULL, sizeof *globalSocketList, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); /* init shared memory */
	makeLSocket(globalSocketList);

	pid_t pid = fork();
	if (pid > 0) {
		// ici pid = PID{process_fils
		runMainThread();
		closeChat(listeningSocket, 0);
	} else if (pid == PROCESSUS_FILS) { // new thread: listeningSocket, pid = 0
		runListeningSocketThread(listeningSocket);
	} else {
		fprintf(stderr, "[serverTCP] Erreur lors de la création du processus serveur.\n");
	}
	rmLSocket(globalSocketList);
	munmap(globalSocketList, sizeof *globalSocketList);
}

void closeChat (int listeningSocket, pid_t listeningSocketPid) {
	//TODO: sendMessage ("le serveur ferme ses portes");
	printf("[closeChat] Fermeture du chat\n");
	h_close(listeningSocket);
	kill(0, SIGTERM);
	// closeSocket(listeningSocketPid, listeningSocket);
}

void registerSocket (pid_t pid, int socket) {
	setSocket(globalSocketList, socket);
	printf("[registerSocket] Nombre de socket : %d\n", getLength(*globalSocketList));
}
void closeSocketClient (pid_t pid, int socket) {
	rmSocket(globalSocketList, socket);
	printf("[closeSocketClient] Nombre de socket : %d\n", getLength(*globalSocketList));
	h_close(socket);
}

void closeSocket(pid_t p, int numSocket) {
	h_close(numSocket); /* fermeture de la socket en attente */
	// kill(p, SIGUSR1); /* kill child process, need sudo if SIGKILL */
	kill(0, SIGTERM);
}

void runMainThread () {
	char stop[5];
	printf("[runMainThread] Entrez [%s] pour arrêter Le processus.\n", EXIT_CHAR);
	do {
		setMessage(stop);
	}	while (!isFlag(stop, EXIT_CHAR));
}

int createListeningSocket(char* port) {
	/* SOCK_STREAM = TCP */
	int numSocket = h_socket(AF_INET, SOCK_STREAM); /* création de la socket */
	struct sockaddr_in *p_adr_serveur;
	adr_socket(port, NULL, SOCK_STREAM, &p_adr_serveur); /* création de l'adresse de la socket */
	if (h_bind(numSocket, p_adr_serveur) != -1) {
		h_listen(numSocket, NB_CON);
	}
	return numSocket;
}

void runListeningSocketThread (int listeningSocket) {
	struct sockaddr_in clientIp;
	while (1) {
		int newSocket = waitForNewConnection(listeningSocket, &clientIp);
		handleNewConnection(newSocket, clientIp);
	}
}

int waitForNewConnection (int listeningSocket, struct sockaddr_in* clientIp) {
	int newSocket = 0;
	while ((newSocket = h_accept(listeningSocket, clientIp)) == -1) {
		fprintf(stderr, "[waitForNewConnection] Nombre de connexions complet.\n");
	}
	registerSocket(getpid(), newSocket);
	return newSocket;
}

void handleNewConnection (int dedicatedSocket, struct sockaddr_in clientSocket) {
	printf( "[handleNewConnection] Handling new connection to socket %d\n", dedicatedSocket);
	pid_t pid = fork();
	if (pid > 0) { // main thread : listeningSocket
		//registerSocket(pid, dedicatedSocket);
	} else if (pid == PROCESSUS_FILS) { // new thread: clientDedicatedSocket
		char clientName[BUFFER_SIZE] = "";
		registerClient(dedicatedSocket, clientSocket, clientName);
		handleClient(dedicatedSocket, clientSocket, clientName);
		closeSocketClient(getpid(), dedicatedSocket);	// TODO: Faux: pid = 0 quand on est dans le fils
	} else {
		fprintf(stderr, "[handleNewConnection] Erreur lors de la création du processus du client.\n");
	}
}

void registerClient (int dedicatedSocket, struct sockaddr_in clientSocAddr, char* clientName) {
	char clientIp[INET_ADDRSTRLEN];
	parseClientName(dedicatedSocket, clientName);
	parseClientIp(clientSocAddr, clientIp);
	// TODO sendToAll();
	printf("[registerClient] %s (%s) entre dans le chat.\n", clientName, clientIp);
}

void parseClientName (int socketClient, char* clientName) {
	sendMessage(socketClient, "Bienvenue dans le client de chat !\nVeuillez entrez votre pseudo : ");
	int nbOctRecus = h_reads(socketClient, bufferReception, BUFFER_SIZE); /* lecture du message pseudo */
	if (nbOctRecus == -1) {
		throwSocketReceptionError();
	} else {
		strcpy(clientName, bufferReception);
	}
}

void parseClientIp (struct sockaddr_in p_adr_client, char *ipAddr) {
	inet_ntop(AF_INET, &p_adr_client.sin_addr, ipAddr, INET_ADDRSTRLEN);
}

void handleClient (int dedicatedSocket, struct sockaddr_in clientIp, char* clientName) {
	while (readClientInput(dedicatedSocket, clientIp, clientName)) {
		// TODO: sendToAll()
		//h_writes(dedicatedSocket, bufferEmission, BUFFER_SIZE);
		sendToAll(*globalSocketList, bufferEmission, BUFFER_SIZE);
	}
	processClientLogout(clientName);
	sendMessage(dedicatedSocket, "\nA bientôt !\nMerci d'avoir utiliser le chat !\n");
}

int readClientInput (int dedicatedSocket, struct sockaddr_in clientIp, char* clientName) {
	sendMessage(dedicatedSocket, "\nVotre message : ");
	int nbOctRecus = h_reads(dedicatedSocket, bufferReception, BUFFER_SIZE); /* lecture du message avant espaces */
	if (nbOctRecus == -1) {
		throwSocketReceptionError();
		return -1;
	} else {
		sprintf(bufferEmission, "%s : %s\n", clientName, bufferReception);
		printf("[readClientInput] %s says \"%s\"\n", clientName, bufferReception);
	}
	return !isFlag(bufferReception, EXIT_CHAR);
}

void processClientLogout (char* clientName) {
	printf("[processClientLogout] %s quitte le chat.\n", clientName);
}
