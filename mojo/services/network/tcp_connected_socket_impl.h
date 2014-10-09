// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_
#define MOJO_SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/services/public/interfaces/network/tcp_connected_socket.mojom.h"
#include "net/socket/tcp_socket.h"

namespace mojo {

class TCPConnectedSocketImpl : public InterfaceImpl<TCPConnectedSocket> {
 public:
  TCPConnectedSocketImpl(scoped_ptr<net::TCPSocket> socket,
                         ScopedDataPipeConsumerHandle send_stream,
                         ScopedDataPipeProducerHandle receive_stream);
  virtual ~TCPConnectedSocketImpl();

 private:
  scoped_ptr<net::TCPSocket> socket_;
  ScopedDataPipeConsumerHandle send_stream_;
  ScopedDataPipeProducerHandle receive_stream_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_
