// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/tcp_bound_socket_impl.h"

#include <utility>

#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/net_address_type_converters.h"
#include "mojo/services/network/tcp_connected_socket_impl.h"
#include "mojo/services/network/tcp_server_socket_impl.h"
#include "net/base/net_errors.h"

namespace mojo {

TCPBoundSocketImpl::TCPBoundSocketImpl(
    scoped_ptr<mojo::AppRefCount> app_refcount,
    InterfaceRequest<TCPBoundSocket> request)
    : app_refcount_(std::move(app_refcount)),
      binding_(this, std::move(request)) {}

TCPBoundSocketImpl::~TCPBoundSocketImpl() {
}

int TCPBoundSocketImpl::Bind(NetAddressPtr local_address) {
  net::IPEndPoint end_point = local_address.To<net::IPEndPoint>();

  socket_.reset(new net::TCPSocket(NULL, net::NetLog::Source()));
  int result = socket_->Open(end_point.GetFamily());
  if (result != net::OK)
    return result;

  result = socket_->SetDefaultOptionsForServer();
  if (result != net::OK)
    return result;

  result = socket_->Bind(end_point);
  if (result != net::OK)
    return result;

  return net::OK;
}

NetAddressPtr TCPBoundSocketImpl::GetLocalAddress() const {
  net::IPEndPoint resolved_local_address;
  if (socket_->GetLocalAddress(&resolved_local_address) != net::OK)
    return NetAddressPtr();
  return NetAddress::From(resolved_local_address);
}

void TCPBoundSocketImpl::StartListening(
    InterfaceRequest<TCPServerSocket> server,
    const Callback<void(NetworkErrorPtr)>& callback) {
  if (!socket_ || pending_connect_socket_.is_pending()) {
    // A bound socket will only be returned to the caller after binding
    // succeeds, so if the socket doesn't exist, that means ownership was
    // already passed to a server socket or client socket.
    callback.Run(MakeNetworkError(net::ERR_FAILED));
    return;
  }

  // TODO(brettw) set the backlog properly.
  int result = socket_->Listen(4);
  if (result != net::OK) {
    callback.Run(MakeNetworkError(result));
    return;
  }

  // The server socket object takes ownership of the socket.
  new TCPServerSocketImpl(std::move(socket_), app_refcount_->Clone(),
                          std::move(server));
  callback.Run(MakeNetworkError(net::OK));
}

void TCPBoundSocketImpl::Connect(
    NetAddressPtr remote_address,
    ScopedDataPipeConsumerHandle send_stream,
    ScopedDataPipeProducerHandle receive_stream,
    InterfaceRequest<TCPConnectedSocket> client_socket,
    const Callback<void(NetworkErrorPtr)>& callback) {
  if (!socket_ || pending_connect_socket_.is_pending()) {
    // A bound socket will only be returned to the caller after binding
    // succeeds, so if the socket doesn't exist, that means ownership was
    // already passed to a server socket or client socket.
    callback.Run(MakeNetworkError(net::ERR_FAILED));
    return;
  }

  net::IPEndPoint end_point = remote_address.To<net::IPEndPoint>();

  pending_connect_send_stream_ = std::move(send_stream);
  pending_connect_receive_stream_ = std::move(receive_stream);
  pending_connect_socket_ = std::move(client_socket);
  pending_connect_callback_ = callback;
  int result = socket_->Connect(
      end_point,
      base::Bind(&TCPBoundSocketImpl::OnConnected, base::Unretained(this)));
  if (result == net::OK) {
    OnConnected(result);
  } else if (result != net::ERR_IO_PENDING) {
    // Error occurred.
    pending_connect_send_stream_.reset();
    pending_connect_receive_stream_.reset();
    pending_connect_socket_ = InterfaceRequest<TCPConnectedSocket>();
    pending_connect_callback_ = Callback<void(NetworkErrorPtr)>();
    callback.Run(MakeNetworkError(result));
  }
}

void TCPBoundSocketImpl::OnConnected(int result) {
  if (result == net::OK) {
    new TCPConnectedSocketImpl(
        std::move(socket_), std::move(pending_connect_send_stream_),
        std::move(pending_connect_receive_stream_),
        std::move(pending_connect_socket_), app_refcount_->Clone());
  } else {
    pending_connect_send_stream_.reset();
    pending_connect_receive_stream_.reset();
    pending_connect_socket_ = InterfaceRequest<TCPConnectedSocket>();
  }
  pending_connect_callback_.Run(MakeNetworkError(result));
  pending_connect_callback_ = Callback<void(NetworkErrorPtr)>();
}

}  // namespace mojo
