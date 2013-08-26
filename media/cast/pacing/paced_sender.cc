// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/pacing/paced_sender.h"

namespace media {
namespace cast {

static const int64 kPacingIntervalMs = 10;
static const int64 kPacingMinIntervalMs = 7;
static const int kPacingMaxBurstsPerFrame = 3;

PacedSender::PacedSender(PacketSender* transport)
    : burst_size_(1),
      packets_sent_in_burst_(0),
      transport_(transport),
      default_tick_clock_(new base::DefaultTickClock()),
      clock_(default_tick_clock_.get()) {
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
  return transport_->SendPacket(&(packet[0]), packet.size());
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
  return transport_->SendPacket(&(packet[0]), packet.size());
}

bool PacedSender::SendRtcpPacket(const std::vector<uint8>& packet) {
  // We pass the RTCP packets straight through.
  return transport_->SendPacket(&(packet[0]), packet.size());
}

base::TimeTicks PacedSender::TimeNextProcess() {
  return time_last_process_ +
      base::TimeDelta::FromMilliseconds(kPacingIntervalMs);
}

void PacedSender::Process() {
  int packets_to_send = 0;
  base::TimeTicks now = clock_->NowTicks();

  base::TimeDelta min_pacing_interval =
    base::TimeDelta::FromMilliseconds(kPacingMinIntervalMs);

  // Have enough time have passed?
  if (now - time_last_process_ < min_pacing_interval)  return;

  time_last_process_ = now;
  packets_to_send = burst_size_;
  // Allow new packets to be inserted while we loop over our packets to send.
  for (int i = 0; i < packets_to_send; ++i) {
    SendStoredPacket();
  }
}

void PacedSender::SendStoredPacket() {
  if (packet_list_.empty() && resend_packet_list_.empty())  return;

  if (!resend_packet_list_.empty()) {
    // Send our re-send packets first.
    const std::vector<uint8>& packet = resend_packet_list_.front();
    transport_->SendPacket(&(packet[0]), packet.size());
    resend_packet_list_.pop_front();
  } else {
    const std::vector<uint8>& packet = packet_list_.front();
    transport_->SendPacket(&(packet[0]), packet.size());
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
