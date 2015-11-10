// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/tcp_client_transport.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/net/stream_socket_connection.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"

namespace blimp {

TCPClientTransport::TCPClientTransport(const net::AddressList& addresses,
                                       net::NetLog* net_log)
    : addresses_(addresses), net_log_(net_log) {}

TCPClientTransport::~TCPClientTransport() {}

int TCPClientTransport::Connect(const net::CompletionCallback& callback) {
  DCHECK(!socket_);
  DCHECK(!callback.is_null());

  socket_.reset(
      new net::TCPClientSocket(addresses_, net_log_, net::NetLog::Source()));
  net::CompletionCallback completion_callback = base::Bind(
      &TCPClientTransport::OnTCPConnectComplete, base::Unretained(this));

  int result = socket_->Connect(completion_callback);
  if (result == net::ERR_IO_PENDING) {
    connect_callback_ = callback;
  } else if (result != net::OK) {
    socket_ = nullptr;
  }

  return result;
}

scoped_ptr<BlimpConnection> TCPClientTransport::TakeConnection() {
  DCHECK(connect_callback_.is_null());
  DCHECK(socket_);
  return make_scoped_ptr(new StreamSocketConnection(socket_.Pass()));
}

void TCPClientTransport::OnTCPConnectComplete(int result) {
  DCHECK(socket_);
  if (result != net::OK) {
    socket_ = nullptr;
  }
  base::ResetAndReturn(&connect_callback_).Run(result);
}

}  // namespace blimp
