// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
      is_connected_(false),
      read_buffer_(new net::IOBufferWithSize(kMaxRead)) {}

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

void Socket::OnDataRead(int result) {
  std::string message;
  if (result >= 0)
    message = std::string(read_buffer_->data(), result);
  event_notifier_->OnDataRead(result, message);
}

void Socket::OnWriteComplete(int result) {
  event_notifier_->OnWriteComplete(result);
}

std::string Socket::Read() {
  int result = datagram_client_socket_->Read(read_buffer_, kMaxRead,
      base::Bind(&Socket::OnDataRead, base::Unretained(this)));
  if (result == net::ERR_IO_PENDING)
    return "";
  if (result < 0)
    return "";
  return std::string(read_buffer_->data(), result);
}

int Socket::Write(const std::string message) {
  int length = message.length();
  scoped_refptr<net::StringIOBuffer> io_buffer(
      new net::StringIOBuffer(message));
  scoped_refptr<net::DrainableIOBuffer> buffer(
      new net::DrainableIOBuffer(io_buffer, length));

  int bytes_sent = 0;
  while (buffer->BytesRemaining()) {
    int result = datagram_client_socket_->Write(
        buffer, buffer->BytesRemaining(),
        base::Bind(&Socket::OnWriteComplete, base::Unretained(this)));
    if (result <= 0)
      // We pass all errors, including ERROR_IO_PENDING, back to the caller.
      return bytes_sent > 0 ? bytes_sent : result;
    bytes_sent += result;
    buffer->DidConsume(result);
  }
  return bytes_sent;
}

}  // namespace extensions
