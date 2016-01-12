// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_TCP_SERVER_SOCKET_H_
#define MOJO_SERVICES_NETWORK_TCP_SERVER_SOCKET_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/network/public/interfaces/tcp_server_socket.mojom.h"
#include "mojo/shell/public/cpp/app_lifetime_helper.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/tcp_socket.h"

namespace mojo {

class TCPServerSocketImpl : public TCPServerSocket {
 public:
  typedef Callback<void(NetworkErrorPtr, NetAddressPtr)> AcceptCallback;

  // Passed ownership of a socket already in listening mode.
  TCPServerSocketImpl(scoped_ptr<net::TCPSocket> socket,
                      scoped_ptr<mojo::AppRefCount> app_refcount,
                      InterfaceRequest<TCPServerSocket> request);
  ~TCPServerSocketImpl() override;

  // TCPServerSocket.
  void Accept(ScopedDataPipeConsumerHandle send_stream,
              ScopedDataPipeProducerHandle receive_stream,
              InterfaceRequest<TCPConnectedSocket> client_socket,
              const AcceptCallback& callback) override;

 private:
  void OnAcceptCompleted(int result);

  scoped_ptr<net::TCPSocket> socket_;

  // Non-null when accept is pending.
  AcceptCallback pending_callback_;

  // The parameters associated with the pending Accept call.
  ScopedDataPipeConsumerHandle pending_send_stream_;
  ScopedDataPipeProducerHandle pending_receive_stream_;
  InterfaceRequest<TCPConnectedSocket> pending_client_socket_;

  // These are written to by net::TCPSocket when Accept is completed.
  scoped_ptr<net::TCPSocket> accepted_socket_;
  net::IPEndPoint accepted_address_;

  scoped_ptr<mojo::AppRefCount> app_refcount_;
  StrongBinding<TCPServerSocket> binding_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_TCP_SERVER_SOCKET_H_
