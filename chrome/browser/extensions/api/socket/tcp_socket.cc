// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/tcp_socket.h"

#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/socket/tcp_client_socket.h"

namespace extensions {

TCPSocket::TCPSocket(const std::string& address, int port,
                     APIResourceEventNotifier* event_notifier)
    : Socket(address, port, event_notifier) {
  net::IPAddressNumber ip_number;
  if (net::ParseIPLiteralToNumber(address, &ip_number)) {
    net::AddressList address_list =
        net::AddressList::CreateFromIPAddress(ip_number, port);
    socket_.reset(new net::TCPClientSocket(address_list, NULL,
                                           net::NetLog::Source()));
  }
}

// For testing.
TCPSocket::TCPSocket(net::TCPClientSocket* tcp_client_socket,
                     const std::string& address, int port,
                     APIResourceEventNotifier* event_notifier)
    : Socket(address, port, event_notifier),
      socket_(tcp_client_socket) {
}

// static
TCPSocket* TCPSocket::CreateSocketForTesting(
    net::TCPClientSocket* tcp_client_socket,
    const std::string& address, int port,
    APIResourceEventNotifier* event_notifier) {
  return new TCPSocket(tcp_client_socket, address, port, event_notifier);
}

TCPSocket::~TCPSocket() {
  if (is_connected_) {
    Disconnect();
  }
}

bool TCPSocket::IsValid() {
  return socket_ != NULL;
}

net::Socket* TCPSocket::socket() {
  return socket_.get();
}

int TCPSocket::Connect() {
  int result = socket_->Connect(base::Bind(
      &TCPSocket::OnConnect, base::Unretained(this)));
  is_connected_ = result == net::OK;
  return result;
}

void TCPSocket::Disconnect() {
  is_connected_ = false;
  socket_->Disconnect();
}

void TCPSocket::OnConnect(int result) {
  is_connected_ = result == net::OK;
  event_notifier()->OnConnectComplete(result);
}

}  // namespace extensions
