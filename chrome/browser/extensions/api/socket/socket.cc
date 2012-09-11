// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket.h"

#include "base/bind.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"

namespace extensions {

Socket::Socket(const std::string& owner_extension_id,
               ApiResourceEventNotifier* event_notifier)
    : ApiResource(owner_extension_id, event_notifier),
      port_(0),
      is_connected_(false) {
}

Socket::~Socket() {
  // Derived destructors should make sure the socket has been closed.
  DCHECK(!is_connected_);
}

void Socket::Write(scoped_refptr<net::IOBuffer> io_buffer,
                   int byte_count,
                   const CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  write_queue_.push(WriteRequest(io_buffer, byte_count, callback));
  WriteData();
}

void Socket::WriteData() {
  // IO is pending.
  if (io_buffer_write_.get())
    return;

  WriteRequest& request = write_queue_.front();

  DCHECK(request.byte_count > request.bytes_written);
  io_buffer_write_ = new net::WrappedIOBuffer(
      request.io_buffer->data() + request.bytes_written);
  int result = WriteImpl(
      io_buffer_write_.get(),
      request.byte_count - request.bytes_written,
      base::Bind(&Socket::OnWriteComplete, base::Unretained(this)));

  if (result != net::ERR_IO_PENDING)
    OnWriteComplete(result);
}

void Socket::OnWriteComplete(int result) {
  io_buffer_write_ = NULL;

  WriteRequest& request = write_queue_.front();

  if (result >= 0) {
    request.bytes_written += result;
    if (request.bytes_written < request.byte_count) {
      WriteData();
      return;
    }
    DCHECK(request.bytes_written == request.byte_count);
    result = request.bytes_written;
  }

  request.callback.Run(result);
  write_queue_.pop();

  if (!write_queue_.empty())
    WriteData();
}

bool Socket::IsConnected() {
  return is_connected_;
}

bool Socket::SetKeepAlive(bool enable, int delay) {
  return false;
}

bool Socket::SetNoDelay(bool no_delay) {
  return false;
}

// static
bool Socket::StringAndPortToIPEndPoint(const std::string& ip_address_str,
                                       int port,
                                       net::IPEndPoint* ip_end_point) {
  DCHECK(ip_end_point);
  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(ip_address_str, &ip_number))
    return false;

  *ip_end_point = net::IPEndPoint(ip_number, port);
  return true;
}

bool Socket::StringAndPortToAddressList(const std::string& ip_address_str,
                                        int port,
                                        net::AddressList* address_list) {
  DCHECK(address_list);
  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(ip_address_str, &ip_number))
    return false;

  *address_list = net::AddressList::CreateFromIPAddress(ip_number, port);
  return true;
}

void Socket::IPEndPointToStringAndPort(const net::IPEndPoint& address,
                                       std::string* ip_address_str,
                                       int* port) {
  DCHECK(ip_address_str);
  DCHECK(port);
  *ip_address_str = address.ToStringWithoutPort();
  if (ip_address_str->empty()) {
    *port = 0;
  } else {
    *port = address.port();
  }
}

Socket::WriteRequest::WriteRequest(scoped_refptr<net::IOBuffer> io_buffer,
                                   int byte_count,
                                   const CompletionCallback& callback)
    : io_buffer(io_buffer),
      byte_count(byte_count),
      callback(callback),
      bytes_written(0) {
}

Socket::WriteRequest::~WriteRequest() { }

}  // namespace extensions
