// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/pacing/paced_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"

namespace media {
namespace cast {

static const int64 kPacingIntervalMs = 10;
static const int kPacingMaxBurstsPerFrame = 3;

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

bool PacedSender::SendPacket(const std::vector<uint8>& packet,
                             int num_of_packets_in_frame) {
  if (!packet_list_.empty()) {
    // We have a queue put the new packets last in the list.
    packet_list_.push_back(packet);
    UpdateBurstSize(num_of_packets_in_frame);
    return true;
  }
  UpdateBurstSize(num_of_packets_in_frame);

  if (packets_sent_in_burst_ >= burst_size_) {
    packet_list_.push_back(packet);
    return true;
  }
  ++packets_sent_in_burst_;
  return transport_->SendPacket(packet);
}

bool PacedSender::ResendPacket(const std::vector<uint8>& packet,
                               int num_of_packets_to_resend) {
  if (!packet_list_.empty() || !resend_packet_list_.empty()) {
    // We have a queue put the resend packets in the list.
    resend_packet_list_.push_back(packet);
    UpdateBurstSize(num_of_packets_to_resend);
    return true;
  }
  UpdateBurstSize(num_of_packets_to_resend);

  if (packets_sent_in_burst_ >= burst_size_) {
    resend_packet_list_.push_back(packet);
    return true;
  }
  ++packets_sent_in_burst_;
  return transport_->SendPacket(packet);
}

bool PacedSender::SendRtcpPacket(const std::vector<uint8>& packet) {
  // We pass the RTCP packets straight through.
  return transport_->SendPacket(packet);
}

void PacedSender::ScheduleNextSend() {
  base::TimeDelta time_to_next = time_last_process_ -
      cast_environment_->Clock()->NowTicks() +
      base::TimeDelta::FromMilliseconds(kPacingIntervalMs);

  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(0));

  cast_environment_->PostDelayedTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&PacedSender::SendNextPacketBurst, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void PacedSender::SendNextPacketBurst() {
  int packets_to_send = burst_size_;
  time_last_process_ = cast_environment_->Clock()->NowTicks();
  for (int i = 0; i < packets_to_send; ++i) {
    SendStoredPacket();
  }
  ScheduleNextSend();
}

void PacedSender::SendStoredPacket() {
  if (packet_list_.empty() && resend_packet_list_.empty()) return;

  if (!resend_packet_list_.empty()) {
    // Send our re-send packets first.
    const std::vector<uint8>& packet = resend_packet_list_.front();
    transport_->SendPacket(packet);
    resend_packet_list_.pop_front();
  } else {
    const std::vector<uint8>& packet = packet_list_.front();
    transport_->SendPacket(packet);
    packet_list_.pop_front();

    if (packet_list_.empty()) {
      burst_size_ = 1;  // Reset burst size after we sent the last stored packet
      packets_sent_in_burst_ = 0;
    }
  }
}

void PacedSender::UpdateBurstSize(int packets_to_send) {
  packets_to_send = std::max(packets_to_send,
      static_cast<int>(resend_packet_list_.size() + packet_list_.size()));

  packets_to_send += (kPacingMaxBurstsPerFrame - 1);  // Round up.

  burst_size_ = std::max(packets_to_send / kPacingMaxBurstsPerFrame,
                         burst_size_);
}

}  // namespace cast
}  // namespace media
