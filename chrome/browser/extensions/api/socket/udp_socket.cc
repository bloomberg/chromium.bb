// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/udp_socket.h"

#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/udp/datagram_socket.h"
#include "net/udp/udp_client_socket.h"

namespace extensions {

UDPSocket::UDPSocket(APIResourceEventNotifier* event_notifier)
    : Socket(event_notifier),
      socket_(net::DatagramSocket::DEFAULT_BIND,
              net::RandIntCallback(),
              NULL,
              net::NetLog::Source()) {
}

UDPSocket::~UDPSocket() {
  if (is_connected_) {
    Disconnect();
  }
}

int UDPSocket::Connect(const std::string& address, int port) {
  if (is_connected_)
    return net::ERR_CONNECTION_FAILED;

  net::IPEndPoint ip_end_point;
  if (!StringAndPortToIPEndPoint(address, port, &ip_end_point))
    return net::ERR_INVALID_ARGUMENT;

  int result = socket_.Connect(ip_end_point);
  is_connected_ = (result == net::OK);
  return result;
}

int UDPSocket::Bind(const std::string& address, int port) {
  net::IPEndPoint ip_end_point;
  if (!StringAndPortToIPEndPoint(address, port, &ip_end_point))
    return net::ERR_INVALID_ARGUMENT;

  return socket_.Bind(ip_end_point);
}

void UDPSocket::Disconnect() {
  is_connected_ = false;
  socket_.Close();
}

int UDPSocket::Read(scoped_refptr<net::IOBuffer> io_buffer, int io_buffer_len) {
  if (!socket_.is_connected())
    return net::ERR_SOCKET_NOT_CONNECTED;

  return socket_.Read(
      io_buffer.get(),
      io_buffer_len,
      base::Bind(&Socket::OnDataRead, base::Unretained(this), io_buffer,
          static_cast<net::IPEndPoint*>(NULL)));
}

int UDPSocket::Write(scoped_refptr<net::IOBuffer> io_buffer, int byte_count) {
  if (!socket_.is_connected())
    return net::ERR_SOCKET_NOT_CONNECTED;

  return socket_.Write(
      io_buffer.get(), byte_count,
      base::Bind(&Socket::OnWriteComplete, base::Unretained(this)));
}

int UDPSocket::RecvFrom(scoped_refptr<net::IOBuffer> io_buffer,
                        int io_buffer_len,
                        net::IPEndPoint* address) {
  if (!socket_.is_connected())
    return net::ERR_SOCKET_NOT_CONNECTED;

  return socket_.RecvFrom(
      io_buffer.get(),
      io_buffer_len,
      address,
      base::Bind(&Socket::OnDataRead, base::Unretained(this), io_buffer,
          address));
}

int UDPSocket::SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                      int byte_count,
                      const std::string& address,
                      int port) {
  net::IPEndPoint ip_end_point;
  if (!StringAndPortToIPEndPoint(address, port, &ip_end_point))
    return net::ERR_INVALID_ARGUMENT;

  if (!socket_.is_connected())
    return net::ERR_SOCKET_NOT_CONNECTED;

  return socket_.SendTo(
      io_buffer.get(),
      byte_count,
      ip_end_point,
      base::Bind(&Socket::OnWriteComplete, base::Unretained(this)));
}

}  // namespace extensions
