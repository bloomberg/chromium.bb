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

UDPSocket::UDPSocket(const std::string& owner_extension_id,
                     ApiResourceEventNotifier* event_notifier)
    : Socket(owner_extension_id, event_notifier),
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

void UDPSocket::Connect(const std::string& address,
                        int port,
                        const CompletionCallback& callback) {
  int result = net::ERR_CONNECTION_FAILED;
  do {
    if (is_connected_)
      break;

    net::IPEndPoint ip_end_point;
    if (!StringAndPortToIPEndPoint(address, port, &ip_end_point)) {
      result = net::ERR_ADDRESS_INVALID;
      break;
    }

    result = socket_.Connect(ip_end_point);
    is_connected_ = (result == net::OK);
  } while (false);

  callback.Run(result);
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

void UDPSocket::Read(int count,
                     const ReadCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (!read_callback_.is_null()) {
    callback.Run(net::ERR_IO_PENDING, NULL);
    return;
  } else {
    read_callback_ = callback;
  }

  int result = net::ERR_FAILED;
  scoped_refptr<net::IOBuffer> io_buffer;
  do {
    if (count < 0) {
      result = net::ERR_INVALID_ARGUMENT;
      break;
    }

    if (!socket_.is_connected()) {
      result = net::ERR_SOCKET_NOT_CONNECTED;
      break;
    }

    io_buffer = new net::IOBuffer(count);
    result = socket_.Read(io_buffer.get(), count,
        base::Bind(&UDPSocket::OnReadComplete, base::Unretained(this),
            io_buffer));
  } while (false);

  if (result != net::ERR_IO_PENDING)
    OnReadComplete(io_buffer, result);
}

int UDPSocket::WriteImpl(net::IOBuffer* io_buffer,
                         int io_buffer_size,
                         const net::CompletionCallback& callback) {
  if (!socket_.is_connected())
    return net::ERR_SOCKET_NOT_CONNECTED;
  else
    return socket_.Write(io_buffer, io_buffer_size, callback);
}

void UDPSocket::RecvFrom(int count,
                         const RecvFromCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (!recv_from_callback_.is_null()) {
    callback.Run(net::ERR_IO_PENDING, NULL, "", 0);
    return;
  } else {
    recv_from_callback_ = callback;
  }

  int result = net::ERR_FAILED;
  scoped_refptr<net::IOBuffer> io_buffer;
  scoped_refptr<IPEndPoint> address;
  do {
    if (count < 0) {
      result = net::ERR_INVALID_ARGUMENT;
      break;
    }

    if (!socket_.is_connected()) {
      result = net::ERR_SOCKET_NOT_CONNECTED;
      break;
    }

    io_buffer = new net::IOBuffer(count);
    address = new IPEndPoint();
    result = socket_.RecvFrom(
        io_buffer.get(),
        count,
        &address->data,
        base::Bind(&UDPSocket::OnRecvFromComplete,
                   base::Unretained(this),
                   io_buffer,
                   address));
  } while (false);

  if (result != net::ERR_IO_PENDING)
    OnRecvFromComplete(io_buffer, address, result);
}

void UDPSocket::SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                       int byte_count,
                       const std::string& address,
                       int port,
                       const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (!send_to_callback_.is_null()) {
    // TODO(penghuang): Put requests in a pending queue to support multiple
    // sendTo calls.
    callback.Run(net::ERR_IO_PENDING);
    return;
  } else {
    send_to_callback_ = callback;
  }

  int result = net::ERR_FAILED;
  do {
    net::IPEndPoint ip_end_point;
    if (!StringAndPortToIPEndPoint(address, port, &ip_end_point)) {
      result =  net::ERR_ADDRESS_INVALID;
      break;
    }

    if (!socket_.is_connected()) {
      result = net::ERR_SOCKET_NOT_CONNECTED;
      break;
    }

    result = socket_.SendTo(
        io_buffer.get(),
        byte_count,
        ip_end_point,
        base::Bind(&UDPSocket::OnSendToComplete, base::Unretained(this)));
  } while (false);

  if (result != net::ERR_IO_PENDING)
    OnSendToComplete(result);
}

bool UDPSocket::GetPeerAddress(net::IPEndPoint* address) {
  return !socket_.GetPeerAddress(address);
}

bool UDPSocket::GetLocalAddress(net::IPEndPoint* address) {
  return !socket_.GetLocalAddress(address);
}

Socket::SocketType UDPSocket::GetSocketType() const {
  return Socket::TYPE_UDP;
}

void UDPSocket::OnReadComplete(scoped_refptr<net::IOBuffer> io_buffer,
                               int result) {
  DCHECK(!read_callback_.is_null());
  read_callback_.Run(result, io_buffer);
  read_callback_.Reset();
}

void UDPSocket::OnRecvFromComplete(scoped_refptr<net::IOBuffer> io_buffer,
                                   scoped_refptr<IPEndPoint> address,
                                   int result) {
  DCHECK(!recv_from_callback_.is_null());
  std::string ip;
  int port = 0;
  if (result > 0 && address.get()) {
    IPEndPointToStringAndPort(address->data, &ip, &port);
  }
  recv_from_callback_.Run(result, io_buffer, ip, port);
  recv_from_callback_.Reset();
}

void UDPSocket::OnSendToComplete(int result) {
  DCHECK(!send_to_callback_.is_null());
  send_to_callback_.Run(result);
  send_to_callback_.Reset();
}

}  // namespace extensions
