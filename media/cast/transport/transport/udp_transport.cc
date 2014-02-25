// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/transport/udp_transport.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"

namespace media {
namespace cast {
namespace transport {

namespace {
const int kMaxPacketSize = 1500;

bool IsEmpty(const net::IPEndPoint& addr) {
  net::IPAddressNumber empty_addr(addr.address().size());
  return std::equal(
             empty_addr.begin(), empty_addr.end(), addr.address().begin()) &&
         !addr.port();
}

bool IsEqual(const net::IPEndPoint& addr1, const net::IPEndPoint& addr2) {
  return addr1.port() == addr2.port() && std::equal(addr1.address().begin(),
                                                    addr1.address().end(),
                                                    addr2.address().begin());
}
}  // namespace

UdpTransport::UdpTransport(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_proxy,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    const CastTransportStatusCallback& status_callback)
    : io_thread_proxy_(io_thread_proxy),
      local_addr_(local_end_point),
      remote_addr_(remote_end_point),
      udp_socket_(new net::UDPSocket(net::DatagramSocket::DEFAULT_BIND,
                                     net::RandIntCallback(),
                                     NULL,
                                     net::NetLog::Source())),
      send_pending_(false),
      recv_buf_(new net::IOBuffer(kMaxPacketSize)),
      status_callback_(status_callback),
      weak_factory_(this) {
  DCHECK(!IsEmpty(local_end_point) || !IsEmpty(remote_end_point));
}

UdpTransport::~UdpTransport() {}

void UdpTransport::StartReceiving(
    const PacketReceiverCallback& packet_receiver) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  packet_receiver_ = packet_receiver;
  udp_socket_->AllowAddressReuse();
  udp_socket_->SetMulticastLoopbackMode(true);
  if (!IsEmpty(local_addr_)) {
    if (udp_socket_->Bind(local_addr_) < 0) {
      status_callback_.Run(TRANSPORT_SOCKET_ERROR);
      LOG(ERROR) << "Failed to bind local address.";
      return;
    }
  } else if (!IsEmpty(remote_addr_)) {
    if (udp_socket_->Connect(remote_addr_) < 0) {
      status_callback_.Run(TRANSPORT_SOCKET_ERROR);
      LOG(ERROR) << "Failed to connect to remote address.";
      return;
    }
  } else {
    NOTREACHED() << "Either local or remote address has to be defined.";
  }
  ReceiveOnePacket();
}

void UdpTransport::ReceiveOnePacket() {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  int result = udp_socket_->RecvFrom(
      recv_buf_,
      kMaxPacketSize,
      &recv_addr_,
      base::Bind(&UdpTransport::OnReceived, weak_factory_.GetWeakPtr()));
  if (result > 0) {
    OnReceived(result);
  } else if (result != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Failed to receive packet: " << result << "."
               << " Stop receiving packets.";
    status_callback_.Run(TRANSPORT_SOCKET_ERROR);
  }
}

void UdpTransport::OnReceived(int result) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
  if (result < 0) {
    LOG(ERROR) << "Failed to receive packet: " << result << "."
               << " Stop receiving packets.";
    status_callback_.Run(TRANSPORT_SOCKET_ERROR);
    return;
  }

  if (IsEmpty(remote_addr_)) {
    remote_addr_ = recv_addr_;
    VLOG(1) << "First packet received from: " << remote_addr_.ToString() << ".";
  } else if (!IsEqual(remote_addr_, recv_addr_)) {
    LOG(ERROR) << "Received from an unrecognized address: "
               << recv_addr_.ToString() << ".";
    return;
  }
  // TODO(hclam): The interfaces should use net::IOBuffer to eliminate memcpy.
  scoped_ptr<Packet> packet(
      new Packet(recv_buf_->data(), recv_buf_->data() + result));
  packet_receiver_.Run(packet.Pass());

  // TODO(hguihot): Read packet iteratively when crbug.com/344628 is fixed.
  io_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&UdpTransport::ReceiveOnePacket, weak_factory_.GetWeakPtr()));
}

bool UdpTransport::SendPacket(const Packet& packet) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  if (send_pending_) {
    VLOG(1) << "Cannot send because of pending IO.";
    return false;
  }

  // TODO(hclam): This interface should take a net::IOBuffer to minimize
  // memcpy.
  scoped_refptr<net::IOBuffer> buf =
      new net::IOBuffer(static_cast<int>(packet.size()));
  memcpy(buf->data(), &packet[0], packet.size());
  int ret = udp_socket_->SendTo(
      buf,
      static_cast<int>(packet.size()),
      remote_addr_,
      base::Bind(&UdpTransport::OnSent, weak_factory_.GetWeakPtr(), buf));
  if (ret == net::ERR_IO_PENDING)
    send_pending_ = true;
  // When ok, will return a positive value equal the number of bytes sent.
  return ret >= net::OK;
}

void UdpTransport::OnSent(const scoped_refptr<net::IOBuffer>& buf, int result) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  send_pending_ = false;
  if (result < 0) {
    LOG(ERROR) << "Failed to send packet: " << result << ".";
    status_callback_.Run(TRANSPORT_SOCKET_ERROR);
  }
}

}  // namespace transport
}  // namespace cast
}  // namespace media
