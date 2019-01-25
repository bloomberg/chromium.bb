// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mock_mdns_socket_factory.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/dns/public/util.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

using testing::_;
using testing::Invoke;

namespace net {

MockMDnsDatagramServerSocket::MockMDnsDatagramServerSocket(
    AddressFamily address_family) {
  local_address_ = dns_util::GetMdnsReceiveEndPoint(address_family);
}

MockMDnsDatagramServerSocket::~MockMDnsDatagramServerSocket() = default;

int MockMDnsDatagramServerSocket::SendTo(IOBuffer* buf,
                                         int buf_len,
                                         const IPEndPoint& address,
                                         CompletionOnceCallback callback) {
  return SendToInternal(std::string(buf->data(), buf_len), address.ToString(),
                        &callback);
}

int MockMDnsDatagramServerSocket::RecvFrom(IOBuffer* buffer,
                                           int size,
                                           IPEndPoint* address,
                                           CompletionOnceCallback callback) {
  return RecvFromInternal(buffer, size, address, &callback);
}

int MockMDnsDatagramServerSocket::GetLocalAddress(IPEndPoint* address) const {
  *address = local_address_;
  return OK;
}

void MockMDnsDatagramServerSocket::SetResponsePacket(
    const std::string& response_packet) {
  response_packet_ = response_packet;
}

int MockMDnsDatagramServerSocket::HandleRecvNow(
    IOBuffer* buffer,
    int size,
    IPEndPoint* address,
    CompletionOnceCallback* callback) {
  int size_returned =
      std::min(response_packet_.size(), static_cast<size_t>(size));
  memcpy(buffer->data(), response_packet_.data(), size_returned);
  return size_returned;
}

int MockMDnsDatagramServerSocket::HandleRecvLater(
    IOBuffer* buffer,
    int size,
    IPEndPoint* address,
    CompletionOnceCallback* callback) {
  int rv = HandleRecvNow(buffer, size, address, callback);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*callback), rv));
  return ERR_IO_PENDING;
}

MockMDnsDatagramClientSocket::MockMDnsDatagramClientSocket() = default;

MockMDnsDatagramClientSocket::~MockMDnsDatagramClientSocket() = default;

int MockMDnsDatagramClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return WriteInternal(std::string(buf->data(), buf_len), &callback,
                       traffic_annotation);
}

MockMDnsSocketFactory::MockMDnsSocketFactory() = default;

MockMDnsSocketFactory::~MockMDnsSocketFactory() = default;

void MockMDnsSocketFactory::CreateSocketPairs(
    std::vector<MDnsSendRecvSocketPair>* socket_pairs) {
  CreateSocketPair(ADDRESS_FAMILY_IPV4, socket_pairs);
  CreateSocketPair(ADDRESS_FAMILY_IPV6, socket_pairs);
}

void MockMDnsSocketFactory::CreateSocketPair(
    AddressFamily address_family,
    std::vector<MDnsSendRecvSocketPair>* socket_pairs) {
  auto new_send_socket =
      std::make_unique<testing::NiceMock<MockMDnsDatagramClientSocket>>();
  auto new_recv_socket =
      std::make_unique<testing::NiceMock<MockMDnsDatagramServerSocket>>(
          address_family);

  ON_CALL(*new_send_socket, WriteInternal(_, _, _))
      .WillByDefault(Invoke(this, &MockMDnsSocketFactory::WriteInternal));

  ON_CALL(*new_recv_socket, RecvFromInternal(_, _, _, _))
      .WillByDefault(Invoke(this, &MockMDnsSocketFactory::RecvFromInternal));

  socket_pairs->push_back(
      std::make_pair(std::move(new_send_socket), std::move(new_recv_socket)));
}

void MockMDnsSocketFactory::SimulateReceive(const uint8_t* packet, int size) {
  DCHECK(recv_buffer_size_ >= size);
  DCHECK(recv_buffer_.get());
  DCHECK(!recv_callback_.is_null());

  memcpy(recv_buffer_->data(), packet, size);
  base::ResetAndReturn(&recv_callback_).Run(size);
}

int MockMDnsSocketFactory::RecvFromInternal(IOBuffer* buffer,
                                            int size,
                                            IPEndPoint* address,
                                            CompletionOnceCallback* callback) {
  recv_buffer_ = buffer;
  recv_buffer_size_ = size;
  recv_callback_ = std::move(*callback);
  return ERR_IO_PENDING;
}

int MockMDnsSocketFactory::WriteInternal(
    const std::string& packet,
    CompletionOnceCallback* callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  OnSendTo(packet);
  return packet.size();
}

}  // namespace net
