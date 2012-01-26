// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/tcp_socket.h"

#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_client_socket.h"

namespace extensions {

TCPSocket::TCPSocket(net::TCPClientSocket* tcp_client_socket,
                     SocketEventNotifier* event_notifier)
: Socket(event_notifier),
  socket_(tcp_client_socket) {
}

TCPSocket::~TCPSocket() {
  if (is_connected_) {
    Disconnect();
  }
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
  event_notifier_->OnConnectComplete(result);
}

}  // namespace extensions
