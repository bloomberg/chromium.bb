// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_TCP_BOUND_SOCKET_H_
#define MOJO_SERVICES_NETWORK_TCP_BOUND_SOCKET_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/services/public/interfaces/network/tcp_bound_socket.mojom.h"
#include "net/socket/tcp_socket.h"

namespace mojo {

class TCPBoundSocketImpl : public InterfaceImpl<TCPBoundSocket> {
 public:
  TCPBoundSocketImpl();
  virtual ~TCPBoundSocketImpl();

  int Bind(NetAddressPtr local_address);

  // Returns the local address associated with this socket. This should only
  // be called after Bind has succeeded.
  NetAddressPtr GetLocalAddress() const;

  // TCPBoundSocket.
  virtual void StartListening(
      InterfaceRequest<TCPServerSocket> server,
      const Callback<void(NetworkErrorPtr)>& callback) override;
  virtual void Connect(
      NetAddressPtr remote_address,
      ScopedDataPipeConsumerHandle send_stream,
      ScopedDataPipeProducerHandle receive_stream,
      InterfaceRequest<TCPConnectedSocket> client_socket,
      const Callback<void(NetworkErrorPtr)>& callback) override;

 private:
  scoped_ptr<net::TCPSocket> socket_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_TCP_BOUND_SOCKET_H_
