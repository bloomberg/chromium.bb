// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/fix_rate_sender.h"

#include <math.h>

#include "base/logging.h"
#include "net/quic/quic_protocol.h"

namespace {
  const int kInitialBitrate = 100000;  // In bytes per second.
  const uint64 kWindowSizeUs = 10000;  // 10 ms.
}

namespace net {

FixRateSender::FixRateSender(const QuicClock* clock)
    : bitrate_(QuicBandwidth::FromBytesPerSecond(kInitialBitrate)),
      fix_rate_leaky_bucket_(clock, bitrate_),
      paced_sender_(clock, bitrate_),
      data_in_flight_(0) {
  DLOG(INFO) << "FixRateSender";
}

void FixRateSender::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& feedback,
    const SentPacketsMap& /*sent_packets*/) {
  DCHECK(feedback.type == kFixRate) <<
      "Invalid incoming CongestionFeedbackType:" << feedback.type;
  if (feedback.type == kFixRate) {
    bitrate_ = feedback.fix_rate.bitrate;
    fix_rate_leaky_bucket_.SetDrainingRate(bitrate_);
    paced_sender_.UpdateBandwidthEstimate(bitrate_);
  }
  // Silently ignore invalid messages in release mode.
}

void FixRateSender::OnIncomingAck(
    QuicPacketSequenceNumber /*acked_sequence_number*/,
    QuicByteCount bytes_acked,
    QuicTime::Delta /*rtt*/) {
  data_in_flight_ -= bytes_acked;
}

void FixRateSender::OnIncomingLoss(int /*number_of_lost_packets*/) {
  // Ignore losses for fix rate sender.
}

void FixRateSender::SentPacket(QuicPacketSequenceNumber /*sequence_number*/,
                               QuicByteCount bytes,
                               bool is_retransmission) {
  fix_rate_leaky_bucket_.Add(bytes);
  paced_sender_.SentPacket(bytes);
  if (!is_retransmission) {
    data_in_flight_ += bytes;
  }
}

QuicTime::Delta FixRateSender::TimeUntilSend(bool /*is_retransmission*/) {
  if (CongestionWindow() > fix_rate_leaky_bucket_.BytesPending()) {
    if (CongestionWindow() <= data_in_flight_) {
      // We need an ack before we send more.
      return QuicTime::Delta::Infinite();
    }
    return paced_sender_.TimeUntilSend(QuicTime::Delta::Zero());
  }
  QuicTime::Delta time_remaining = fix_rate_leaky_bucket_.TimeRemaining();
  if (time_remaining.IsZero()) {
    // We need an ack before we send more.
    return QuicTime::Delta::Infinite();
  }
  return paced_sender_.TimeUntilSend(time_remaining);
}

QuicByteCount FixRateSender::CongestionWindow() {
  QuicByteCount window_size_bytes = bitrate_.ToBytesPerPeriod(
      QuicTime::Delta::FromMicroseconds(kWindowSizeUs));
  // Make sure window size is not less than a packet.
  return std::max(kMaxPacketSize, window_size_bytes);
}

QuicByteCount FixRateSender::AvailableCongestionWindow() {
  QuicByteCount congestion_window = CongestionWindow();
  if (data_in_flight_ >= congestion_window) {
    return 0;
  }
  QuicByteCount available_congestion_window = congestion_window -
      data_in_flight_;
  return paced_sender_.AvailableWindow(available_congestion_window);
}

QuicBandwidth FixRateSender::BandwidthEstimate() {
  return bitrate_;
}

}  // namespace net
