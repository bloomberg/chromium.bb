// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket.h"

#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/udp/datagram_socket.h"

namespace extensions {

Socket::Socket(net::DatagramClientSocket* datagram_client_socket,
               SocketEventNotifier* event_notifier)
    : datagram_client_socket_(datagram_client_socket),
      event_notifier_(event_notifier),
      is_connected_(false) {}

Socket::~Socket() {
  if (is_connected_) {
    Close();
  }
}

bool Socket::Connect(const net::IPEndPoint& ip_end_point) {
  is_connected_ = datagram_client_socket_->Connect(ip_end_point) == net::OK;
  return is_connected_;
}

void Socket::Close() {
  is_connected_ = false;
  datagram_client_socket_->Close();
}

void Socket::OnWriteComplete(int result) {
  event_notifier_->OnEvent(SOCKET_EVENT_WRITE_COMPLETE, result);
}

int Socket::Write(const std::string message) {
  int length = message.length();
  scoped_refptr<net::StringIOBuffer> io_buffer(
      new net::StringIOBuffer(message));
  scoped_refptr<net::DrainableIOBuffer> buffer(
      new net::DrainableIOBuffer(io_buffer, length));

  int bytes_sent = 0;
  while (buffer->BytesRemaining()) {
    int rv = datagram_client_socket_->Write(
        buffer, buffer->BytesRemaining(),
        base::Bind(&Socket::OnWriteComplete, base::Unretained(this)));
    if (rv <= 0) {
      // We pass all errors, including ERROR_IO_PENDING, back to the caller.
      return bytes_sent > 0 ? bytes_sent : rv;
    }
    bytes_sent += rv;
    buffer->DidConsume(rv);
  }
  return bytes_sent;
}

}  // namespace extensions
