// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket_api_controller.h"

#include "chrome/browser/extensions/api/socket/tcp_socket.h"
#include "chrome/browser/extensions/api/socket/udp_socket.h"
#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/udp/datagram_socket.h"
#include "net/udp/udp_client_socket.h"

namespace extensions {

SocketController::SocketController() : next_socket_id_(1) {}

SocketController::~SocketController() {}

Socket* SocketController::GetSocket(int socket_id) {
  // TODO(miket): we should verify that the extension asking for the
  // socket is the same one that created it.
  SocketMap::iterator i = socket_map_.find(socket_id);
  if (i != socket_map_.end())
    return i->second.get();
  return NULL;
}

// TODO(miket): we should consider partitioning the ID space by extension ID
// to make it harder for extensions to peek into each others' sockets.
int SocketController::GenerateSocketId() {
  while (next_socket_id_ > 0 && socket_map_.count(next_socket_id_) > 0)
    ++next_socket_id_;
  return next_socket_id_++;
}

int SocketController::CreateTCPSocket(const std::string& address, int port,
                                      SocketEventNotifier* event_notifier) {
  net::IPAddressNumber ip_number;
  bool result = net::ParseIPLiteralToNumber(address, &ip_number);
  if (!result)
    return 0;

  net::AddressList address_list =
      net::AddressList::CreateFromIPAddress(ip_number, port);
  linked_ptr<Socket> socket(new TCPSocket(
      new net::TCPClientSocket(address_list, NULL, net::NetLog::Source()),
      event_notifier));
  CHECK(socket.get());
  int next_socket_id = GenerateSocketId();
  if (next_socket_id > 0) {
    socket_map_[next_socket_id] = socket;
    return next_socket_id;
  }
  return 0;
}

int SocketController::CreateUDPSocket(const std::string& address, int port,
                                      SocketEventNotifier* event_notifier) {
  linked_ptr<Socket> socket(new UDPSocket(
      new net::UDPClientSocket(net::DatagramSocket::DEFAULT_BIND,
                               net::RandIntCallback(),
                               NULL,
                               net::NetLog::Source()),
      address, port, event_notifier));
  CHECK(socket.get());
  int next_socket_id = GenerateSocketId();
  if (next_socket_id > 0) {
    socket_map_[next_socket_id] = socket;
    return next_socket_id;
  }
  return 0;
}

bool SocketController::DestroySocket(int socket_id) {
  Socket* socket = GetSocket(socket_id);
  if (!socket)
    return false;
  delete socket;
  socket_map_.erase(socket_id);
  return true;
}

int SocketController::ConnectSocket(int socket_id) {
  Socket* socket = GetSocket(socket_id);
  if (socket)
    return socket->Connect();
  return net::ERR_FILE_NOT_FOUND;
}

void SocketController::DisconnectSocket(int socket_id) {
  Socket* socket = GetSocket(socket_id);
  if (socket)
    socket->Disconnect();
}

std::string SocketController::ReadSocket(int socket_id) {
  Socket* socket = GetSocket(socket_id);
  if (!socket)
    return "";
  return socket->Read();
}

int SocketController::WriteSocket(int socket_id, const std::string& message) {
  Socket* socket = GetSocket(socket_id);
  if (!socket)
    return -1;
  return socket->Write(message);
}

}  // namespace extensions
