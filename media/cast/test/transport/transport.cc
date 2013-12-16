// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/transport/transport.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
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

class LocalUdpTransportData
    : public base::RefCountedThreadSafe<LocalUdpTransportData> {
 public:
  LocalUdpTransportData(net::UDPServerSocket* udp_socket,
                        scoped_refptr<base::TaskRunner> io_thread_proxy)
    : udp_socket_(udp_socket),
      buffer_(new net::IOBufferWithSize(kMaxPacketSize)),
      io_thread_proxy_(io_thread_proxy) {
  }

  void ListenTo(net::IPEndPoint bind_address) {
    DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

    bind_address_ = bind_address;
    io_thread_proxy_->PostTask(FROM_HERE,
        base::Bind(&LocalUdpTransportData::RecvFromSocketLoop, this));
  }

  void DeletePacket(uint8* data) {
    // Should be called from the receiver (not on the transport thread).
    DCHECK(!(io_thread_proxy_->RunsTasksOnCurrentThread()));
    delete [] data;
  }

  void PacketReceived(int size) {
    DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
    // Got a packet with length result.
    uint8* data = new uint8[size];
    memcpy(data, buffer_->data(), size);
    packet_receiver_->ReceivedPacket(data, size,
        base::Bind(&LocalUdpTransportData::DeletePacket, this, data));
    RecvFromSocketLoop();

  }

  void RecvFromSocketLoop() {
    DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
    // Callback should always trigger with a packet.
    int res = udp_socket_->RecvFrom(buffer_.get(), kMaxPacketSize,
        &bind_address_, base::Bind(&LocalUdpTransportData::PacketReceived,
        this));
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

 protected:
  virtual ~LocalUdpTransportData() {}

 private:
  friend class base::RefCountedThreadSafe<LocalUdpTransportData>;

  net::UDPServerSocket* udp_socket_;
  net::IPEndPoint bind_address_;
  PacketReceiver* packet_receiver_;
  scoped_refptr<net::IOBufferWithSize> buffer_;
  scoped_refptr<base::TaskRunner> io_thread_proxy_;

  DISALLOW_COPY_AND_ASSIGN(LocalUdpTransportData);
};

class LocalPacketSender : public PacketSender,
                          public base::RefCountedThreadSafe<LocalPacketSender> {
 public:
  LocalPacketSender(net::UDPServerSocket* udp_socket,
                    scoped_refptr<base::TaskRunner> io_thread_proxy)
      : udp_socket_(udp_socket),
        send_address_(),
        loss_limit_(0),
        io_thread_proxy_(io_thread_proxy) {}

  virtual bool SendPacket(const Packet& packet) OVERRIDE {
    io_thread_proxy_->PostTask(FROM_HERE,
    base::Bind(&LocalPacketSender::SendPacketToNetwork, this, packet));
    return true;
  }

  virtual void SendPacketToNetwork(const Packet& packet) {
    DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
    const uint8* data = packet.data();
    if (loss_limit_ > 0) {
      int r = base::RandInt(0, 100);
      if (r < loss_limit_) {
        VLOG(1) << "Drop packet f:" << static_cast<int>(data[12 + 1])
                << " p:" << static_cast<int>(data[12 + 3])
                << " m:" << static_cast<int>(data[12 + 5]);
        return;
      }
    }
    net::TestCompletionCallback callback;
    scoped_refptr<net::WrappedIOBuffer> buffer(
        new net::WrappedIOBuffer(reinterpret_cast<const char*>(data)));
    udp_socket_->SendTo(buffer.get(), static_cast<int>(packet.size()),
                        send_address_, callback.callback());
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
    DCHECK_GE(percentage, 0);
    DCHECK_LT(percentage, 100);
    loss_limit_ = percentage;
  }

  void SetSendAddress(const net::IPEndPoint& send_address) {
    send_address_ = send_address;
  }

 protected:
  virtual ~LocalPacketSender() {}

 private:
  friend class base::RefCountedThreadSafe<LocalPacketSender>;

  net::UDPServerSocket* udp_socket_;  // Not owned by this class.
  net::IPEndPoint send_address_;
  int loss_limit_;
  scoped_refptr<base::TaskRunner> io_thread_proxy_;
};

Transport::Transport(
    scoped_refptr<base::TaskRunner> io_thread_proxy)
    : udp_socket_(new net::UDPServerSocket(NULL, net::NetLog::Source())),
      local_udp_transport_data_(new LocalUdpTransportData(udp_socket_.get(),
                                                          io_thread_proxy)),
      packet_sender_(new LocalPacketSender(udp_socket_.get(), io_thread_proxy)),
      io_thread_proxy_(io_thread_proxy) {}

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
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
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
