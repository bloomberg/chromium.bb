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

Socket::Socket(APIResourceEventNotifier* event_notifier)
    : APIResource(APIResource::SocketResource, event_notifier),
      port_(0),
      is_connected_(false) {
}

Socket::~Socket() {
  // Derived destructors should make sure the socket has been closed.
  DCHECK(!is_connected_);
}

void Socket::OnDataRead(scoped_refptr<net::IOBuffer> io_buffer,
                        net::IPEndPoint* address,
                        int result) {
  // OnDataRead will take ownership of data_value.
  ListValue* data_value = new ListValue();
  if (result >= 0) {
    size_t bytes_size = static_cast<size_t>(result);
    const char* io_buffer_start = io_buffer->data();
    for (size_t i = 0; i < bytes_size; ++i) {
      data_value->Set(i, Value::CreateIntegerValue(io_buffer_start[i]));
    }
  }

  std::string ip_address_str;
  int port = 0;
  if (address)
    IPEndPointToStringAndPort(*address, &ip_address_str, &port);
  event_notifier()->OnDataRead(result, data_value, ip_address_str, port);
}

void Socket::OnWriteComplete(int result) {
  event_notifier()->OnWriteComplete(result);
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
  struct sockaddr_storage addr;
  size_t addr_len = sizeof(addr);
  if (address.ToSockAddr(reinterpret_cast<struct sockaddr*>(&addr),
                         &addr_len)) {
    *ip_address_str = net::NetAddressToString(
        reinterpret_cast<struct sockaddr*>(&addr), addr_len);
    *port = address.port();
  } else {
    *ip_address_str = "";
    *port = 0;
  }
}

}  // namespace extensions
