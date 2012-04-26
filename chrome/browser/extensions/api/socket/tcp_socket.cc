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

TCPSocket::TCPSocket(APIResourceEventNotifier* event_notifier)
    : Socket(event_notifier) {
}

// For testing.
TCPSocket::TCPSocket(net::TCPClientSocket* tcp_client_socket,
                     APIResourceEventNotifier* event_notifier)
    : Socket(event_notifier),
      socket_(tcp_client_socket) {
}

// static
TCPSocket* TCPSocket::CreateSocketForTesting(
    net::TCPClientSocket* tcp_client_socket,
    APIResourceEventNotifier* event_notifier) {
  return new TCPSocket(tcp_client_socket, event_notifier);
}

TCPSocket::~TCPSocket() {
  if (is_connected_) {
    Disconnect();
  }
}

int TCPSocket::Connect(const std::string& address, int port) {
  if (is_connected_)
    return net::ERR_CONNECTION_FAILED;

  net::AddressList address_list;
  if (!StringAndPortToAddressList(address, port, &address_list))
    return net::ERR_INVALID_ARGUMENT;

  socket_.reset(new net::TCPClientSocket(address_list, NULL,
                                         net::NetLog::Source()));

  int result = socket_->Connect(base::Bind(
      &TCPSocket::OnConnect, base::Unretained(this)));
  if (result == net::OK) {
    is_connected_ = true;
  }
  return result;
}

void TCPSocket::Disconnect() {
  is_connected_ = false;
  socket_->Disconnect();
}

int TCPSocket::Bind(const std::string& address, int port) {
  // TODO(penghuang): Supports bind for tcp?
  return net::ERR_FAILED;
}

int TCPSocket::Read(scoped_refptr<net::IOBuffer> io_buffer, int io_buffer_len) {
  if (!socket_.get() || !socket_->IsConnected())
    return net::ERR_SOCKET_NOT_CONNECTED;

  return socket_->Read(
      io_buffer.get(),
      io_buffer_len,
      base::Bind(&Socket::OnDataRead, base::Unretained(this), io_buffer,
          static_cast<net::IPEndPoint*>(NULL)));
}

int TCPSocket::Write(scoped_refptr<net::IOBuffer> io_buffer, int byte_count) {
  if (!socket_.get() || !socket_->IsConnected())
    return net::ERR_SOCKET_NOT_CONNECTED;

  return socket_->Write(
      io_buffer.get(), byte_count,
      base::Bind(&Socket::OnWriteComplete, base::Unretained(this)));
}

int TCPSocket::RecvFrom(scoped_refptr<net::IOBuffer> io_buffer,
                        int io_buffer_len,
                        net::IPEndPoint* address) {
  return net::ERR_FAILED;
}

int TCPSocket::SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                      int byte_count,
                      const std::string& address,
                      int port) {
  return net::ERR_FAILED;
}

void TCPSocket::OnConnect(int result) {
  is_connected_ = result == net::OK;
  event_notifier()->OnConnectComplete(result);
}

}  // namespace extensions
