// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket_api_controller.h"

#include "chrome/browser/extensions/api/socket/socket.h"
#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"
#include "net/base/rand_callback.h"
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

int SocketController::CreateUdp(SocketEventNotifier* event_notifier) {
  linked_ptr<Socket> socket(new Socket(
      new net::UDPClientSocket(net::DatagramSocket::DEFAULT_BIND,
                               net::RandIntCallback(),
                               NULL,
                               net::NetLog::Source()), event_notifier));
  CHECK(socket.get());
  socket_map_[next_socket_id_] = socket;
  return next_socket_id_++;
}

bool SocketController::DestroyUdp(int socket_id) {
  Socket* socket = GetSocket(socket_id);
  if (!socket)
    return false;
  delete socket;
  socket_map_.erase(socket_id);
  return true;
}

// TODO(miket): it *might* be nice to be able to resolve DNS. I am not putting
// in interesting error reporting for this method because we clearly can't
// leave experimental without DNS resolution.
//
// static
bool SocketController::CreateIPEndPoint(const std::string& address, int port,
                                        net::IPEndPoint* ip_end_point) {
  net::IPAddressNumber ip_number;
  bool result = net::ParseIPLiteralToNumber(address, &ip_number);
  if (!result)
    return false;
  *ip_end_point = net::IPEndPoint(ip_number, port);
  return true;
}

bool SocketController::ConnectUdp(int socket_id, const std::string& address,
                                  int port) {
  Socket* socket = GetSocket(socket_id);
  if (!socket)
    return false;
  net::IPEndPoint ip_end_point;
  if (!CreateIPEndPoint(address, port, &ip_end_point))
    return false;
  return socket->Connect(ip_end_point);
}

void SocketController::CloseUdp(int socket_id) {
  Socket* socket = GetSocket(socket_id);
  if (socket)
    socket->Close();
}

std::string SocketController::ReadUdp(int socket_id) {
  Socket* socket = GetSocket(socket_id);
  if (!socket)
    return "";
  return socket->Read();
}

int SocketController::WriteUdp(int socket_id, const std::string& message) {
  Socket* socket = GetSocket(socket_id);
  if (!socket)
    return -1;
  return socket->Write(message);
}

}  // namespace extensions
