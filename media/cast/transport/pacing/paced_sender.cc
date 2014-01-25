// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/pacing/paced_sender.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace media {
namespace cast {
namespace transport {

namespace {
static const int64 kPacingIntervalMs = 10;
// Each frame will be split into no more than kPacingMaxBurstsPerFrame
// bursts of packets.
static const size_t kPacingMaxBurstsPerFrame = 3;

void CreateUDPAddress(std::string ip_str, int port, net::IPEndPoint* address) {
  net::IPAddressNumber ip_number;
  bool rv = net::ParseIPLiteralToNumber(ip_str, &ip_number);
  if (!rv)
    return;
  *address = net::IPEndPoint(ip_number, port);
}
}  // namespace

PacedSender::PacedSender(
    base::TickClock* clock,
    const CastTransportConfig* const config,
    PacketSender* external_transport,
    const scoped_refptr<base::TaskRunner>& transport_task_runner,
    const CastTransportStatusCallback& status_callback)
    : clock_(clock),
      external_transport_(external_transport),
      config_(config),
      transport_task_runner_(transport_task_runner),
      burst_size_(1),
      packets_sent_in_burst_(0),
      weak_factory_(this) {
  if (!external_transport) {
    net::IPEndPoint local_end_point;
    net::IPEndPoint receiver_end_point;
    CreateUDPAddress(config_->local_ip_address, config_->receive_port,
                     &local_end_point);
    CreateUDPAddress(config_->receiver_ip_address, config_->send_port,
                     &receiver_end_point);
    // Set up transport in the absence of an external transport.
    transport_.reset(new UdpTransport(transport_task_runner,
                                      local_end_point,
                                      receiver_end_point,
                                      status_callback));
  }
  ScheduleNextSend();
}

PacedSender::~PacedSender() {}

void PacedSender::SetPacketReceiver(
    scoped_refptr<PacketReceiver> packet_receiver) {
  DCHECK(!external_transport_);
  transport_->StartReceiving(packet_receiver.get());
}

bool PacedSender::SendPackets(const PacketList& packets) {
  return SendPacketsToTransport(packets, &packet_list_);
}

bool PacedSender::ResendPackets(const PacketList& packets) {
  return SendPacketsToTransport(packets, &resend_packet_list_);
}

bool PacedSender::SendPacketsToTransport(const PacketList& packets,
                                         PacketList* packets_not_sent) {
  UpdateBurstSize(packets.size());

  if (!packets_not_sent->empty()) {
    packets_not_sent->insert(packets_not_sent->end(),
                             packets.begin(), packets.end());
    return true;
  }

  PacketList packets_to_send;
  PacketList::const_iterator first_to_store_it = packets.begin();

  size_t max_packets_to_send_now = burst_size_ - packets_sent_in_burst_;

  if (max_packets_to_send_now > 0) {
    size_t packets_to_send_now = std::min(max_packets_to_send_now,
                                          packets.size());

    std::advance(first_to_store_it, packets_to_send_now);
    packets_to_send.insert(packets_to_send.begin(),
                           packets.begin(), first_to_store_it);
  }
  packets_not_sent->insert(packets_not_sent->end(),
                         first_to_store_it, packets.end());
  packets_sent_in_burst_ += packets_to_send.size();
  if (packets_to_send.empty()) return true;

  return TransmitPackets(packets_to_send);
}

bool PacedSender::SendRtcpPacket(const Packet& packet) {
  if (external_transport_) {
    return external_transport_->SendPacket(packet);
  }
  // We pass the RTCP packets straight through.
  return transport_->SendPacket(packet);
}

void PacedSender::ScheduleNextSend() {
  base::TimeDelta time_to_next = time_last_process_ -
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(kPacingIntervalMs);

  time_to_next = std::max(time_to_next, base::TimeDelta());

  transport_task_runner_->PostDelayedTask(FROM_HERE,
      base::Bind(&PacedSender::SendNextPacketBurst, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void PacedSender::SendNextPacketBurst() {
  SendStoredPackets();
  time_last_process_ = clock_->NowTicks();
  ScheduleNextSend();
}

void PacedSender::SendStoredPackets() {
  if (packet_list_.empty() && resend_packet_list_.empty()) return;

  size_t packets_to_send = burst_size_;
  PacketList packets_to_resend;

  // Send our re-send packets first.
  if (!resend_packet_list_.empty()) {
    PacketList::iterator it = resend_packet_list_.begin();
    size_t packets_to_send_now = std::min(packets_to_send,
                                          resend_packet_list_.size());
    std::advance(it, packets_to_send_now);
    packets_to_resend.insert(packets_to_resend.begin(),
                             resend_packet_list_.begin(), it);
    resend_packet_list_.erase(resend_packet_list_.begin(), it);
    packets_to_send -= packets_to_resend.size();
  }
  if (!packet_list_.empty() && packets_to_send > 0) {
    PacketList::iterator it = packet_list_.begin();
    size_t packets_to_send_now = std::min(packets_to_send,
                                          packet_list_.size());

    std::advance(it, packets_to_send_now);
    packets_to_resend.insert(packets_to_resend.end(),
                             packet_list_.begin(), it);
    packet_list_.erase(packet_list_.begin(), it);

    if (packet_list_.empty()) {
      burst_size_ = 1;  // Reset burst size after we sent the last stored packet
      packets_sent_in_burst_ = 0;
    }
  }
  TransmitPackets(packets_to_resend);
}

bool PacedSender::TransmitPackets(const PacketList& packets) {
  bool ret = true;
  for (size_t i = 0; i < packets.size(); i++) {
    if (external_transport_) {
      ret &= external_transport_->SendPacket(packets[i]);
    } else {
      ret &= transport_->SendPacket(packets[i]);
    }
  }
  return ret;
}

void PacedSender::UpdateBurstSize(size_t packets_to_send) {
  packets_to_send = std::max(packets_to_send,
      resend_packet_list_.size() + packet_list_.size());

  packets_to_send += (kPacingMaxBurstsPerFrame - 1);  // Round up.
  burst_size_ = std::max(packets_to_send / kPacingMaxBurstsPerFrame,
                         burst_size_);
}

void PacedSender::InsertFakeTransportForTesting(PacketSender* fake_transport) {
  external_transport_ = fake_transport;
}

}  // namespace transport
}  // namespace cast
}  // namespace media
