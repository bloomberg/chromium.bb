// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/transport/transport.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/rand_callback.h"
#include "net/base/test_completion_callback.h"

namespace media {
namespace cast {
namespace test {

const int kMaxPacketSize = 1500;

class LocalUdpTransportData;

void CreateUDPAddress(std::string ip_str, int port, net::IPEndPoint* address) {
  net::IPAddressNumber ip_number;
  bool rv = net::ParseIPLiteralToNumber(ip_str, &ip_number);
  if (!rv)
    return;
  *address = net::IPEndPoint(ip_number, port);
}

class LocalUdpTransportData {
 public:
  LocalUdpTransportData(net::DatagramServerSocket* udp_socket)
    : udp_socket_(udp_socket),
      buffer_(new net::IOBufferWithSize(kMaxPacketSize)),
      weak_factory_(this) {
  }

  void ListenTo(net::IPEndPoint bind_address) {
    bind_address_ = bind_address;
    RecvFromSocketLoop();
  }

  void PacketReceived(int result) {
    // Got a packet with length result.
    uint8* data = reinterpret_cast<uint8*>(buffer_->data());
    packet_receiver_->ReceivedPacket(data, result,
        base::Bind(&LocalUdpTransportData::RecvFromSocketLoop,
        weak_factory_.GetWeakPtr()));
  }

  void RecvFromSocketLoop() {
    // Callback should always trigger with a packet.
    int res = udp_socket_->RecvFrom(buffer_.get(), kMaxPacketSize,
        &bind_address_, base::Bind(&LocalUdpTransportData::PacketReceived,
        weak_factory_.GetWeakPtr()));
    DCHECK(res >= net::ERR_IO_PENDING);
    if (res > 0) {
      PacketReceived(res);
    }
  }

  void set_packet_receiver(PacketReceiver* packet_receiver) {
    packet_receiver_ = packet_receiver;
  }

  void Close() {
    udp_socket_->Close();
  }

 private:
  net::DatagramServerSocket* udp_socket_;
  net::IPEndPoint bind_address_;
  PacketReceiver* packet_receiver_;
  scoped_refptr<net::IOBufferWithSize> buffer_;
  base::WeakPtrFactory<LocalUdpTransportData> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(LocalUdpTransportData);
};

class LocalPacketSender : public PacketSender {
 public:
  explicit LocalPacketSender(net::DatagramServerSocket* udp_socket)
      : udp_socket_(udp_socket),
        send_address_(),
        loss_limit_(0) {}

  virtual bool SendPacket(const Packet& packet) OVERRIDE {
    const uint8* data = packet.data();
    if (loss_limit_ > 0) {
      int r = base::RandInt(0, 100);
      if (r < loss_limit_) {
        VLOG(1) << "Drop packet f:" << static_cast<int>(data[12 + 1])
                << " p:" << static_cast<int>(data[12 + 3])
                << " m:" << static_cast<int>(data[12 + 5]);
        return true;
      }
    }
    net::TestCompletionCallback callback;
    scoped_refptr<net::WrappedIOBuffer> buffer(
        new net::WrappedIOBuffer(reinterpret_cast<const char*>(data)));
    int rv = udp_socket_->SendTo(
        buffer.get(), static_cast<int>(packet.size()), send_address_,
        callback.callback());
    return (rv == packet.size());
  }

  virtual bool SendPackets(const PacketList& packets) OVERRIDE {
    bool out_val = true;
    for (size_t i = 0; i < packets.size(); ++i) {
      const Packet& packet = packets[i];
      out_val |= SendPacket(packet);
    }
    return out_val;
  }

  void SetPacketLoss(int percentage) {
    DCHECK(percentage >= 0);
    DCHECK(percentage < 100);
    loss_limit_ = percentage;
  }

  void SetSendAddress(net::IPEndPoint& send_address) {
    send_address_ = send_address;
  }

 private:
  net::DatagramServerSocket* udp_socket_;  // Not owned by this class.
  net::IPEndPoint send_address_;
  int loss_limit_;
};

Transport::Transport(scoped_refptr<CastEnvironment> cast_environment)
    : udp_socket_(new net::UDPServerSocket(NULL, net::NetLog::Source())),
      local_udp_transport_data_(new LocalUdpTransportData(udp_socket_.get())),
      packet_sender_(new LocalPacketSender(udp_socket_.get())) {}

Transport::~Transport() {}

PacketSender* Transport::packet_sender() {
  return static_cast<PacketSender*>(packet_sender_.get());
}

void Transport::SetSendSidePacketLoss(int percentage) {
  packet_sender_->SetPacketLoss(percentage);
}

void Transport::StopReceiving() {
  local_udp_transport_data_->Close();
}

void Transport::SetLocalReceiver(PacketReceiver* packet_receiver,
                                 std::string ip_address,
                                 std::string local_ip_address,
                                 int port) {
  net::IPEndPoint bind_address, local_bind_address;
  CreateUDPAddress(ip_address, port, &bind_address);
  CreateUDPAddress(local_ip_address, port, &local_bind_address);
  local_udp_transport_data_->set_packet_receiver(packet_receiver);
  udp_socket_->AllowAddressReuse();
  udp_socket_->SetMulticastLoopbackMode(true);
  udp_socket_->Listen(local_bind_address);

  // Start listening once receiver has been set.
  local_udp_transport_data_->ListenTo(bind_address);
}

void Transport::SetSendDestination(std::string ip_address, int port) {
  net::IPEndPoint send_address;
  CreateUDPAddress(ip_address, port, &send_address);
  packet_sender_->SetSendAddress(send_address);
}

}  // namespace test
}  // namespace cast
}  // namespace media
