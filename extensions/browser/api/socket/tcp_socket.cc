// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/tcp_socket.h"

#include "extensions/browser/api/api_resource.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/socket/tcp_client_socket.h"

namespace extensions {

const char kTCPSocketTypeInvalidError[] =
    "Cannot call both connect and listen on the same socket.";
const char kSocketListenError[] = "Could not listen on the specified port.";

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ApiResourceManager<ResumableTCPSocket> > >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<ResumableTCPSocket> >*
ApiResourceManager<ResumableTCPSocket>::GetFactoryInstance() {
  return g_factory.Pointer();
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<ResumableTCPServerSocket> > > g_server_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<ResumableTCPServerSocket> >*
ApiResourceManager<ResumableTCPServerSocket>::GetFactoryInstance() {
  return g_server_factory.Pointer();
}

TCPSocket::TCPSocket(const std::string& owner_extension_id)
    : Socket(owner_extension_id), socket_mode_(UNKNOWN) {}

TCPSocket::TCPSocket(net::TCPClientSocket* tcp_client_socket,
                     const std::string& owner_extension_id,
                     bool is_connected)
    : Socket(owner_extension_id),
      socket_(tcp_client_socket),
      socket_mode_(CLIENT) {
  this->is_connected_ = is_connected;
}

TCPSocket::TCPSocket(net::TCPServerSocket* tcp_server_socket,
                     const std::string& owner_extension_id)
    : Socket(owner_extension_id),
      server_socket_(tcp_server_socket),
      socket_mode_(SERVER) {}

// static
TCPSocket* TCPSocket::CreateSocketForTesting(
    net::TCPClientSocket* tcp_client_socket,
    const std::string& owner_extension_id,
    bool is_connected) {
  return new TCPSocket(tcp_client_socket, owner_extension_id, is_connected);
}

// static
TCPSocket* TCPSocket::CreateServerSocketForTesting(
    net::TCPServerSocket* tcp_server_socket,
    const std::string& owner_extension_id) {
  return new TCPSocket(tcp_server_socket, owner_extension_id);
}

TCPSocket::~TCPSocket() { Disconnect(); }

void TCPSocket::Connect(const std::string& address,
                        int port,
                        const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (socket_mode_ == SERVER || !connect_callback_.is_null()) {
    callback.Run(net::ERR_CONNECTION_FAILED);
    return;
  }
  DCHECK(!server_socket_.get());
  socket_mode_ = CLIENT;
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

    socket_.reset(
        new net::TCPClientSocket(address_list, NULL, net::NetLog::Source()));

    connect_callback_ = callback;
    result = socket_->Connect(
        base::Bind(&TCPSocket::OnConnectComplete, base::Unretained(this)));
  } while (false);

  if (result != net::ERR_IO_PENDING)
    OnConnectComplete(result);
}

void TCPSocket::Disconnect() {
  is_connected_ = false;
  if (socket_.get())
    socket_->Disconnect();
  server_socket_.reset(NULL);
  connect_callback_.Reset();
  read_callback_.Reset();
  accept_callback_.Reset();
  accept_socket_.reset(NULL);
}

int TCPSocket::Bind(const std::string& address, int port) {
  return net::ERR_FAILED;
}

void TCPSocket::Read(int count, const ReadCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (socket_mode_ != CLIENT) {
    callback.Run(net::ERR_FAILED, NULL);
    return;
  }

  if (!read_callback_.is_null()) {
    callback.Run(net::ERR_IO_PENDING, NULL);
    return;
  }

  if (count < 0) {
    callback.Run(net::ERR_INVALID_ARGUMENT, NULL);
    return;
  }

  if (!socket_.get() || !IsConnected()) {
    callback.Run(net::ERR_SOCKET_NOT_CONNECTED, NULL);
    return;
  }

  read_callback_ = callback;
  scoped_refptr<net::IOBuffer> io_buffer = new net::IOBuffer(count);
  int result = socket_->Read(
      io_buffer.get(),
      count,
      base::Bind(
          &TCPSocket::OnReadComplete, base::Unretained(this), io_buffer));

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

bool TCPSocket::SetKeepAlive(bool enable, int delay) {
  if (!socket_.get())
    return false;
  return socket_->SetKeepAlive(enable, delay);
}

bool TCPSocket::SetNoDelay(bool no_delay) {
  if (!socket_.get())
    return false;
  return socket_->SetNoDelay(no_delay);
}

int TCPSocket::Listen(const std::string& address,
                      int port,
                      int backlog,
                      std::string* error_msg) {
  if (socket_mode_ == CLIENT) {
    *error_msg = kTCPSocketTypeInvalidError;
    return net::ERR_NOT_IMPLEMENTED;
  }
  DCHECK(!socket_.get());
  socket_mode_ = SERVER;

  if (!server_socket_.get()) {
    server_socket_.reset(new net::TCPServerSocket(NULL, net::NetLog::Source()));
  }
  int result = server_socket_->ListenWithAddressAndPort(address, port, backlog);
  if (result)
    *error_msg = kSocketListenError;
  return result;
}

void TCPSocket::Accept(const AcceptCompletionCallback& callback) {
  if (socket_mode_ != SERVER || !server_socket_.get()) {
    callback.Run(net::ERR_FAILED, NULL);
    return;
  }

  // Limits to only 1 blocked accept call.
  if (!accept_callback_.is_null()) {
    callback.Run(net::ERR_FAILED, NULL);
    return;
  }

  int result = server_socket_->Accept(
      &accept_socket_,
      base::Bind(&TCPSocket::OnAccept, base::Unretained(this)));
  if (result == net::ERR_IO_PENDING) {
    accept_callback_ = callback;
  } else if (result == net::OK) {
    accept_callback_ = callback;
    this->OnAccept(result);
  } else {
    callback.Run(result, NULL);
  }
}

bool TCPSocket::IsConnected() {
  RefreshConnectionStatus();
  return is_connected_;
}

bool TCPSocket::GetPeerAddress(net::IPEndPoint* address) {
  if (!socket_.get())
    return false;
  return !socket_->GetPeerAddress(address);
}

bool TCPSocket::GetLocalAddress(net::IPEndPoint* address) {
  if (socket_.get()) {
    return !socket_->GetLocalAddress(address);
  } else if (server_socket_.get()) {
    return !server_socket_->GetLocalAddress(address);
  } else {
    return false;
  }
}

Socket::SocketType TCPSocket::GetSocketType() const { return Socket::TYPE_TCP; }

int TCPSocket::WriteImpl(net::IOBuffer* io_buffer,
                         int io_buffer_size,
                         const net::CompletionCallback& callback) {
  if (socket_mode_ != CLIENT)
    return net::ERR_FAILED;
  else if (!socket_.get() || !IsConnected())
    return net::ERR_SOCKET_NOT_CONNECTED;
  else
    return socket_->Write(io_buffer, io_buffer_size, callback);
}

void TCPSocket::RefreshConnectionStatus() {
  if (!is_connected_)
    return;
  if (server_socket_)
    return;
  if (!socket_->IsConnected()) {
    Disconnect();
  }
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

void TCPSocket::OnAccept(int result) {
  DCHECK(!accept_callback_.is_null());
  if (result == net::OK && accept_socket_.get()) {
    accept_callback_.Run(
        result, static_cast<net::TCPClientSocket*>(accept_socket_.release()));
  } else {
    accept_callback_.Run(result, NULL);
  }
  accept_callback_.Reset();
}

ResumableTCPSocket::ResumableTCPSocket(const std::string& owner_extension_id)
    : TCPSocket(owner_extension_id),
      persistent_(false),
      buffer_size_(0),
      paused_(false) {}

ResumableTCPSocket::ResumableTCPSocket(net::TCPClientSocket* tcp_client_socket,
                                       const std::string& owner_extension_id,
                                       bool is_connected)
    : TCPSocket(tcp_client_socket, owner_extension_id, is_connected),
      persistent_(false),
      buffer_size_(0),
      paused_(false) {}

bool ResumableTCPSocket::IsPersistent() const { return persistent(); }

ResumableTCPServerSocket::ResumableTCPServerSocket(
    const std::string& owner_extension_id)
    : TCPSocket(owner_extension_id), persistent_(false), paused_(false) {}

bool ResumableTCPServerSocket::IsPersistent() const { return persistent(); }

}  // namespace extensions
