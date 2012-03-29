// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket.h"

#include "base/bind.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"

namespace extensions {

Socket::Socket(const std::string& address, int port,
               APIResourceEventNotifier* event_notifier)
    : APIResource(APIResource::SocketResource, event_notifier),
      address_(address),
      port_(port),
      is_connected_(false),
      read_buffer_(new net::IOBufferWithSize(kMaxRead)) {
}

Socket::~Socket() {
  // Derived destructors should make sure the socket has been closed.
  DCHECK(!is_connected_);
}

void Socket::OnDataRead(int result) {
  std::string message;
  if (result >= 0)
    message = std::string(read_buffer_->data(), result);
  event_notifier()->OnDataRead(result, message);
}

void Socket::OnWriteComplete(int result) {
  event_notifier()->OnWriteComplete(result);
}

std::string Socket::Read() {
  int result = socket()->Read(
      read_buffer_, kMaxRead,
      base::Bind(&Socket::OnDataRead, base::Unretained(this)));
  if (result == net::ERR_IO_PENDING)
    return "";
  if (result < 0)
    return "";
  return std::string(read_buffer_->data(), result);
}

int Socket::Write(const std::string& message) {
  int length = message.length();
  scoped_refptr<net::StringIOBuffer> io_buffer(
      new net::StringIOBuffer(message));
  scoped_refptr<net::DrainableIOBuffer> buffer(
      new net::DrainableIOBuffer(io_buffer, length));

  int bytes_sent = 0;
  while (buffer->BytesRemaining()) {
    int result = socket()->Write(
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
