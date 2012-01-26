// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/udp_socket.h"

#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/udp/datagram_socket.h"

namespace extensions {

UDPSocket::UDPSocket(net::DatagramClientSocket* datagram_client_socket,
                     const std::string& address, int port,
                     SocketEventNotifier* event_notifier)
: Socket(event_notifier),
  socket_(datagram_client_socket),
  address_(address),
  port_(port) {
}

UDPSocket::~UDPSocket() {
  if (is_connected_) {
    Disconnect();
  }
}

net::Socket* UDPSocket::socket() {
  return socket_.get();
}

int UDPSocket::Connect() {
  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(address_, &ip_number))
    return net::ERR_INVALID_ARGUMENT;
  int result = socket_->Connect(net::IPEndPoint(ip_number, port_));
  is_connected_ = result == net::OK;
  return result;
}

void UDPSocket::Disconnect() {
  is_connected_ = false;
  socket_->Close();
}

}  // namespace extensions
