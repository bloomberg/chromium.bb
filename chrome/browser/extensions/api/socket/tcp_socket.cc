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

void TCPSocket::Connect(const std::string& address,
                        int port,
                        const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (!connect_callback_.is_null()) {
    callback.Run(net::ERR_CONNECTION_FAILED);
    return;
  }
  connect_callback_ = callback;

  int result = net::ERR_CONNECTION_FAILED;
  do {
    if (is_connected_)
      break;

    net::AddressList address_list;
    if (!StringAndPortToAddressList(address, port, &address_list)) {
      result = net::ERR_ADDRESS_INVALID;
      break;
    }

    socket_.reset(new net::TCPClientSocket(address_list, NULL,
                                           net::NetLog::Source()));
    connect_callback_ = callback;
    result = socket_->Connect(base::Bind(
        &TCPSocket::OnConnectComplete, base::Unretained(this)));
  } while (false);

  if (result != net::ERR_IO_PENDING)
    OnConnectComplete(result);
}

void TCPSocket::Disconnect() {
  is_connected_ = false;
  socket_->Disconnect();
}

int TCPSocket::Bind(const std::string& address, int port) {
  // TODO(penghuang): Supports bind for tcp?
  return net::ERR_FAILED;
}

void TCPSocket::Read(int count,
                     const ReadCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (!read_callback_.is_null()) {
    callback.Run(net::ERR_IO_PENDING, NULL);
    return;
  } else {
    read_callback_ = callback;
  }

  int result = net::ERR_FAILED;
  scoped_refptr<net::IOBuffer> io_buffer;
  do {
    if (count < 0) {
      result = net::ERR_INVALID_ARGUMENT;
      break;
    }

    if (!socket_.get() || !socket_->IsConnected()) {
        result = net::ERR_SOCKET_NOT_CONNECTED;
        break;
    }

    io_buffer = new net::IOBuffer(count);
    result = socket_->Read(io_buffer.get(), count,
        base::Bind(&TCPSocket::OnReadComplete, base::Unretained(this),
            io_buffer));
  } while (false);

  if (result != net::ERR_IO_PENDING)
    OnReadComplete(io_buffer, result);
}

void TCPSocket::RecvFrom(int count,
                         const RecvFromCompletionCallback& callback) {
  callback.Run(net::ERR_FAILED, NULL, NULL, 0);
}

void TCPSocket::SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                       int byte_count,
                       const std::string& address,
                       int port,
                       const CompletionCallback& callback) {
  callback.Run(net::ERR_FAILED);
}

int TCPSocket::WriteImpl(net::IOBuffer* io_buffer,
                         int io_buffer_size,
                         const net::CompletionCallback& callback) {
  if (!socket_.get() || !socket_->IsConnected())
    return net::ERR_SOCKET_NOT_CONNECTED;
  else
    return socket_->Write(io_buffer, io_buffer_size, callback);
}

void TCPSocket::OnConnectComplete(int result) {
  DCHECK(!connect_callback_.is_null());
  DCHECK(!is_connected_);
  is_connected_ = result == net::OK;
  connect_callback_.Run(result);
  connect_callback_.Reset();
}

void TCPSocket::OnReadComplete(scoped_refptr<net::IOBuffer> io_buffer,
                               int result) {
  DCHECK(!read_callback_.is_null());
  read_callback_.Run(result, io_buffer);
  read_callback_.Reset();
}

}  // namespace extensions
