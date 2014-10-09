// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/tcp_connected_socket_impl.h"

namespace mojo {

TCPConnectedSocketImpl::TCPConnectedSocketImpl(
    scoped_ptr<net::TCPSocket> socket,
    ScopedDataPipeConsumerHandle send_stream,
    ScopedDataPipeProducerHandle receive_stream)
    : socket_(socket.Pass()),
      send_stream_(send_stream.Pass()),
      receive_stream_(receive_stream.Pass()) {
}

TCPConnectedSocketImpl::~TCPConnectedSocketImpl() {
}

}  // namespace mojo
