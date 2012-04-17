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
      is_connected_(false) {
}

Socket::~Socket() {
  // Derived destructors should make sure the socket has been closed.
  DCHECK(!is_connected_);
}

void Socket::OnDataRead(scoped_refptr<net::IOBuffer> io_buffer, int result) {
  // OnDataRead will take ownership of data_value.
  ListValue* data_value = new ListValue();
  if (result >= 0) {
    size_t bytes_size = static_cast<size_t>(result);
    const char* io_buffer_start = io_buffer->data();
    for (size_t i = 0; i < bytes_size; ++i) {
      data_value->Set(i, Value::CreateIntegerValue(io_buffer_start[i]));
    }
  }
  event_notifier()->OnDataRead(result, data_value);
}

void Socket::OnWriteComplete(int result) {
  event_notifier()->OnWriteComplete(result);
}

int Socket::Read(scoped_refptr<net::IOBuffer> io_buffer, int io_buffer_len) {
  return socket()->Read(
      io_buffer.get(),
      io_buffer_len,
      base::Bind(&Socket::OnDataRead, base::Unretained(this), io_buffer));
}

int Socket::Write(scoped_refptr<net::IOBuffer> io_buffer, int byte_count) {
  return socket()->Write(
      io_buffer.get(), byte_count,
      base::Bind(&Socket::OnWriteComplete, base::Unretained(this)));
}

}  // namespace extensions
