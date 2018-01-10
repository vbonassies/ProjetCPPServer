// servertest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#undef UNICODE
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <vector>

#pragma comment (lib, "Ws2_32.lib")

#define IP_ADDRESS "192.168.56.1"
#define DEFAULT_PORT "3504"
#define DEFAULT_BUFLEN 512


struct client_type
{
	int id;
	SOCKET socket;
};

const char option_value = 1;
const int max_clients = 5;
int nb_message_user[max_clients];
std::string client_name[max_clients];

//Function Prototypes
int process_client(client_type &new_client, std::vector<client_type> &client_array, std::thread &thread);
int main();

int process_client(client_type &new_client, std::vector<client_type> &client_array, std::thread &thread)
{
	std::string msg;
	char tempmsg[DEFAULT_BUFLEN] = "";

	//Session
	while (true)
	{
		memset(tempmsg, 0, DEFAULT_BUFLEN);

		if (new_client.socket != 0)
		{
			auto i_result = recv(new_client.socket, tempmsg, DEFAULT_BUFLEN, 0);
			
			if (i_result != SOCKET_ERROR)
			{
				if (strcmp("", tempmsg) != 0)
				{
					if(nb_message_user[new_client.id] == 0)
					{
						client_name[new_client.id] = tempmsg;
						nb_message_user[new_client.id] = 1;
						std::cout << "Client #" << new_client.id << " is now known as " << client_name[new_client.id] <<std::endl;
					} else
					{
						msg = client_name[new_client.id] + ": " + tempmsg;
					}
				}
					

				std::cout << msg.c_str() << std::endl;

				//Broadcast that message to the other clients
				for (auto i = 0; i < max_clients; i++)
				{
					if (client_array[i].socket != INVALID_SOCKET)
						if (new_client.id != i)
							i_result = send(client_array[i].socket, msg.c_str(), strlen(msg.c_str()), 0);
				}
			}
			else
			{
				msg = client_name[new_client.id] + " disconnected";

				std::cout << msg << std::endl;

				closesocket(new_client.socket);
				closesocket(client_array[new_client.id].socket);
				client_array[new_client.id].socket = INVALID_SOCKET;

				//Broadcast the disconnection message to the other clients
				for (auto i = 0; i < max_clients; i++)
				{
					if (client_array[i].socket != INVALID_SOCKET)
						i_result = send(client_array[i].socket, msg.c_str(), strlen(msg.c_str()), 0);
				}

				break;
			}
		}
	}

	thread.detach();

	return 0;
}

int main()
{
	WSADATA wsa_data;
	struct addrinfo hints{};
	struct addrinfo *server = nullptr;
	auto server_socket = INVALID_SOCKET;
	std::string msg;
	std::vector<client_type> client(max_clients);
	auto num_clients = 0;
	auto temp_id = -1;
	std::thread my_thread[max_clients];

	//Initialize Winsock
	std::cout << "Intializing Winsock..." << std::endl;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);

	//Setup hints
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//Setup Server
	std::cout << "Setting up server..." << std::endl;
	getaddrinfo(static_cast<LPCTSTR>(IP_ADDRESS), DEFAULT_PORT, &hints, &server);

	//Create a listening socket for connecting to server
	std::cout << "Creating server socket..." << std::endl;
	server_socket = socket(server->ai_family, server->ai_socktype, server->ai_protocol);

	//Setup socket options
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(int)); //Make it possible to re-bind to a port that was used within the last 2 minutes
	setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, &option_value, sizeof(int)); //Used for interactive programs

																					 //Assign an address to the server socket.
	std::cout << "Binding socket..." << std::endl;
	bind(server_socket, server->ai_addr, static_cast<int>(server->ai_addrlen));

	//Listen for incoming connections.
	std::cout << "Listening..." << std::endl;
	listen(server_socket, SOMAXCONN);

	//Initialize the client list
	for (auto i = 0; i < max_clients; i++)
	{
		client[i] = { -1, INVALID_SOCKET };
	}

	while (true)
	{
		auto incoming = INVALID_SOCKET;
		incoming = accept(server_socket, nullptr, nullptr);

		if (incoming == INVALID_SOCKET) continue;

		//Reset the number of clients
		num_clients = -1;

		//Create a temporary id for the next client
		temp_id = -1;
		for (auto i = 0; i < max_clients; i++)
		{
			if (client[i].socket == INVALID_SOCKET && temp_id == -1)
			{
				client[i].socket = incoming;
				client[i].id = i;
				temp_id = i;
			}

			if (client[i].socket != INVALID_SOCKET)
				num_clients++;
		}

		if (temp_id != -1)
		{
			//Send the id to that client
			std::cout << "Client #" << client[temp_id].id << " Accepted" << std::endl;
			msg = std::to_string(client[temp_id].id);
			send(client[temp_id].socket, msg.c_str(), strlen(msg.c_str()), 0);

			//Create a thread process for that client
			my_thread[temp_id] = std::thread(process_client, std::ref(client[temp_id]), std::ref(client), std::ref(my_thread[temp_id]));
		}
		else
		{
			msg = "Server is full";
			send(incoming, msg.c_str(), strlen(msg.c_str()), 0);
			std::cout << msg << std::endl;
		}
	}
}


