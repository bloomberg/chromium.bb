// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/pacing/paced_sender.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace media {
namespace cast {

static const int64 kPacingIntervalMs = 10;
// Each frame will be split into no more than kPacingMaxBurstsPerFrame
// bursts of packets.
static const size_t kPacingMaxBurstsPerFrame = 3;

PacedSender::PacedSender(scoped_refptr<CastEnvironment> cast_environment,
                         PacketSender* transport)
    : cast_environment_(cast_environment),
      burst_size_(1),
      packets_sent_in_burst_(0),
      transport_(transport),
      weak_factory_(this) {
  ScheduleNextSend();
}

PacedSender::~PacedSender() {}

bool PacedSender::SendPackets(const PacketList& packets) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_environment_->Logging()->InsertPacketListEvent(kPacketSentToPacer,
                                                      packets);
  return SendPacketsToTransport(packets, &packet_list_);
}

bool PacedSender::ResendPackets(const PacketList& packets) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_environment_->Logging()->InsertPacketListEvent(kPacketRetransmited,
                                                      packets);
  return SendPacketsToTransport(packets, &resend_packet_list_);
}

bool PacedSender::SendPacketsToTransport(const PacketList& packets,
                                         PacketList* packets_not_sent) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
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

  cast_environment_->Logging()->InsertPacketListEvent(kPacketSentToNetwork,
                                                      packets);
  return transport_->SendPackets(packets_to_send);
}

bool PacedSender::SendRtcpPacket(const Packet& packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // We pass the RTCP packets straight through.
  return transport_->SendPacket(packet);
}

void PacedSender::ScheduleNextSend() {
  base::TimeDelta time_to_next = time_last_process_ -
      cast_environment_->Clock()->NowTicks() +
      base::TimeDelta::FromMilliseconds(kPacingIntervalMs);

  time_to_next = std::max(time_to_next, base::TimeDelta());

  cast_environment_->PostDelayedTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&PacedSender::SendNextPacketBurst, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void PacedSender::SendNextPacketBurst() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  SendStoredPackets();
  time_last_process_ = cast_environment_->Clock()->NowTicks();
  ScheduleNextSend();
}

void PacedSender::SendStoredPackets() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
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
  transport_->SendPackets(packets_to_resend);
}

void PacedSender::UpdateBurstSize(size_t packets_to_send) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  packets_to_send = std::max(packets_to_send,
      resend_packet_list_.size() + packet_list_.size());

  packets_to_send += (kPacingMaxBurstsPerFrame - 1);  // Round up.
  burst_size_ = std::max(packets_to_send / kPacingMaxBurstsPerFrame,
                         burst_size_);
}

}  // namespace cast
}  // namespace media
