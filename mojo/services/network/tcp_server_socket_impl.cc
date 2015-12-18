// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/tcp_server_socket_impl.h"

#include <utility>

#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/net_address_type_converters.h"
#include "mojo/services/network/tcp_connected_socket_impl.h"
#include "net/base/net_errors.h"

namespace mojo {

TCPServerSocketImpl::TCPServerSocketImpl(
    scoped_ptr<net::TCPSocket> socket,
    scoped_ptr<mojo::AppRefCount> app_refcount,
    InterfaceRequest<TCPServerSocket> request)
    : socket_(std::move(socket)),
      app_refcount_(std::move(app_refcount)),
      binding_(this, std::move(request)) {}

TCPServerSocketImpl::~TCPServerSocketImpl() {
}

void TCPServerSocketImpl::Accept(
    ScopedDataPipeConsumerHandle send_stream,
    ScopedDataPipeProducerHandle receive_stream,
    InterfaceRequest<TCPConnectedSocket> client_socket,
    const AcceptCallback& callback) {
  // One possible future enhancement would be to enqueue multiple Accept calls
  // on this object. This would allow the client to accept some number of
  // incoming connections rapidly without doing an IPC round-trip.
  if (!pending_callback_.is_null()) {
    // Already have a pending accept on this socket.
    callback.Run(MakeNetworkError(net::ERR_UNEXPECTED), NetAddressPtr());
    return;
  }

  int result = socket_->Accept(
      &accepted_socket_, &accepted_address_,
      base::Bind(&TCPServerSocketImpl::OnAcceptCompleted,
                 base::Unretained(this)));
  if (result == net::OK || result == net::ERR_IO_PENDING) {
    pending_callback_ = callback;
    pending_send_stream_ = std::move(send_stream);
    pending_receive_stream_ = std::move(receive_stream);
    pending_client_socket_ = std::move(client_socket);
    if (result == net::OK)
      OnAcceptCompleted(net::OK);
  } else {
    callback.Run(MakeNetworkError(result), NetAddressPtr());
  }
}

void TCPServerSocketImpl::OnAcceptCompleted(int result) {
  if (result != net::OK) {
    pending_callback_.Run(MakeNetworkError(result), NetAddressPtr());
    pending_send_stream_.reset();
    pending_receive_stream_.reset();
    pending_client_socket_ = InterfaceRequest<TCPConnectedSocket>();
  } else {
    new TCPConnectedSocketImpl(
        std::move(accepted_socket_), std::move(pending_send_stream_),
        std::move(pending_receive_stream_), std::move(pending_client_socket_),
        app_refcount_->Clone());
    pending_callback_.Run(MakeNetworkError(net::OK),
                          NetAddress::From(accepted_address_));
  }

  pending_callback_ = AcceptCallback();
}

}  // namespace mojo
