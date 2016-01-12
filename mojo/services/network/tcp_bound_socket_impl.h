// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_TCP_BOUND_SOCKET_H_
#define MOJO_SERVICES_NETWORK_TCP_BOUND_SOCKET_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/network/public/interfaces/tcp_bound_socket.mojom.h"
#include "mojo/shell/public/cpp/app_lifetime_helper.h"
#include "net/socket/tcp_socket.h"

namespace mojo {

class TCPBoundSocketImpl : public TCPBoundSocket {
 public:
  TCPBoundSocketImpl(scoped_ptr<mojo::AppRefCount> app_refcount,
                     InterfaceRequest<TCPBoundSocket> request);
  ~TCPBoundSocketImpl() override;

  // Does the actual binding. Returns a net error code. On net::OK, the bound
  // socket will be ready to use and send back to the client. On failure, this
  // object should be destroyed and no longer used.
  int Bind(NetAddressPtr local_address);

  // Returns the local address associated with this socket. This should only
  // be called after Bind has succeeded.
  NetAddressPtr GetLocalAddress() const;

  // TCPBoundSocket.
  void StartListening(InterfaceRequest<TCPServerSocket> server,
                      const Callback<void(NetworkErrorPtr)>& callback) override;
  void Connect(NetAddressPtr remote_address,
               ScopedDataPipeConsumerHandle send_stream,
               ScopedDataPipeProducerHandle receive_stream,
               InterfaceRequest<TCPConnectedSocket> client_socket,
               const Callback<void(NetworkErrorPtr)>& callback) override;

 private:
  void OnConnected(int result);

  scoped_ptr<net::TCPSocket> socket_;

  // Valid when waiting for a connect callback.
  ScopedDataPipeConsumerHandle pending_connect_send_stream_;
  ScopedDataPipeProducerHandle pending_connect_receive_stream_;
  InterfaceRequest<TCPConnectedSocket> pending_connect_socket_;
  Callback<void(NetworkErrorPtr)> pending_connect_callback_;
  scoped_ptr<mojo::AppRefCount> app_refcount_;
  StrongBinding<TCPBoundSocket> binding_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_TCP_BOUND_SOCKET_H_
