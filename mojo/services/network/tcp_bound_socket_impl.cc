// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/tcp_bound_socket_impl.h"

#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/net_address_type_converters.h"
#include "mojo/services/network/tcp_connected_socket_impl.h"
#include "mojo/services/network/tcp_server_socket_impl.h"
#include "net/base/net_errors.h"

namespace mojo {

TCPBoundSocketImpl::TCPBoundSocketImpl() {
}

TCPBoundSocketImpl::~TCPBoundSocketImpl() {
}

int TCPBoundSocketImpl::Bind(NetAddressPtr local_address) {
  // The local address might be null to match any port.
  net::IPEndPoint end_point;
  if (!local_address.is_null())
    end_point = local_address.To<net::IPEndPoint>();

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
  if (!socket_) {
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
  BindToRequest(new TCPServerSocketImpl(socket_.Pass()), &server);
  callback.Run(MakeNetworkError(net::OK));
}

void TCPBoundSocketImpl::Connect(
    NetAddressPtr remote_address,
    ScopedDataPipeConsumerHandle send_stream,
    ScopedDataPipeProducerHandle receive_stream,
    InterfaceRequest<TCPConnectedSocket> client_socket,
    const Callback<void(NetworkErrorPtr)>& callback) {
  // TODO(brettw) write this.
}

}  // namespace mojo
