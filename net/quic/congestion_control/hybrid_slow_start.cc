// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/hybrid_slow_start.h"

#include <algorithm>

#include "net/quic/congestion_control/rtt_stats.h"

using std::max;
using std::min;

namespace net {

// Note(pwestin): the magic clamping numbers come from the original code in
// tcp_cubic.c.
const int64 kHybridStartLowWindow = 16;
// Number of delay samples for detecting the increase of delay.
const uint32 kHybridStartMinSamples = 8;
const int kHybridStartDelayFactorExp = 4;  // 2^4 = 16
const int kHybridStartDelayMinThresholdUs = 2000;
// TODO(ianswett): The original paper specifies 8, not 16ms.
const int kHybridStartDelayMaxThresholdUs = 16000;

HybridSlowStart::HybridSlowStart(const QuicClock* clock)
    : clock_(clock),
      started_(false),
      found_ack_train_(false),
      found_delay_(false),
      round_start_(QuicTime::Zero()),
      update_end_sequence_number_(true),
      sender_end_sequence_number_(0),
      end_sequence_number_(0),
      last_close_ack_pair_time_(QuicTime::Zero()),
      rtt_sample_count_(0),
      current_min_rtt_(QuicTime::Delta::Zero()) {
}

void HybridSlowStart::OnPacketAcked(
    QuicPacketSequenceNumber acked_sequence_number, bool in_slow_start) {
  if (in_slow_start && IsEndOfRound(acked_sequence_number)) {
    Reset(sender_end_sequence_number_);
  }

  if (sender_end_sequence_number_ == acked_sequence_number) {
    DVLOG(1) << "Start update end sequence number @" << acked_sequence_number;
    update_end_sequence_number_ = true;
  }
}

void HybridSlowStart::OnPacketSent(QuicPacketSequenceNumber sequence_number,
                                   QuicByteCount available_send_window) {
  if (update_end_sequence_number_) {
    sender_end_sequence_number_ = sequence_number;
    if (available_send_window == 0) {
      update_end_sequence_number_ = false;
      DVLOG(1) << "Stop update end sequence number @" << sequence_number;
    }
  }
}

bool HybridSlowStart::ShouldExitSlowStart(const RttStats* rtt_stats,
                                          int64 congestion_window) {
  // Hybrid start triggers when cwnd is larger than some threshold.
  if (congestion_window < kHybridStartLowWindow) {
    return false;
  }
  if (!started_) {
    // Time to start the hybrid slow start.
    Reset(sender_end_sequence_number_);
  }
  // TODO(ianswett): Merge UpdateAndMaybeExit into ShouldExitSlowStart in
  // another refactor CL, since it's only used here.
  return UpdateAndMaybeExit(rtt_stats->latest_rtt(), rtt_stats->min_rtt());
}

void HybridSlowStart::Restart() {
  started_ = false;
  found_ack_train_ = false;
  found_delay_  = false;
}

void HybridSlowStart::Reset(QuicPacketSequenceNumber end_sequence_number) {
  DVLOG(1) << "Reset hybrid slow start @" << end_sequence_number;
  round_start_ = last_close_ack_pair_time_ = clock_->ApproximateNow();
  end_sequence_number_ = end_sequence_number;
  current_min_rtt_ = QuicTime::Delta::Zero();
  rtt_sample_count_ = 0;
  started_ = true;
}

bool HybridSlowStart::IsEndOfRound(QuicPacketSequenceNumber ack) const {
  return end_sequence_number_ <= ack;
}

bool HybridSlowStart::UpdateAndMaybeExit(QuicTime::Delta rtt,
                                         QuicTime::Delta min_rtt) {
  // The original code doesn't invoke this until we hit 16 packet per burst.
  // Since the code handles lower than 16 gracefully I removed that limit.
  if (found_ack_train_ || found_delay_) {
    return true;
  }
  QuicTime current_time = clock_->ApproximateNow();

  // First detection parameter - ack-train detection.
  // Since slow start burst out packets we can indirectly estimate the inter-
  // arrival time by looking at the arrival time of the ACKs if the ACKs are
  // spread out more then half the minimum RTT packets are being spread out
  // more than the capacity.
  // This first trigger will not come into play until we hit roughly 9.6 Mbps
  // with delayed acks (or 4.8Mbps without delayed acks)
  // TODO(pwestin): we need to make sure our pacing don't trigger this detector.
  if (current_time.Subtract(last_close_ack_pair_time_).ToMicroseconds() <=
          kHybridStartDelayMinThresholdUs) {
    last_close_ack_pair_time_ = current_time;
    found_ack_train_ = current_time.Subtract(round_start_).ToMicroseconds() >=
        min_rtt.ToMicroseconds() >> 1;
  }
  // Second detection parameter - delay increase detection.
  // Compare the minimum delay (current_min_rtt_) of the current
  // burst of packets relative to the minimum delay during the session.
  // Note: we only look at the first few(8) packets in each burst, since we
  // only want to compare the lowest RTT of the burst relative to previous
  // bursts.
  rtt_sample_count_++;
  if (rtt_sample_count_ <= kHybridStartMinSamples) {
    if (current_min_rtt_.IsZero() || current_min_rtt_ > rtt) {
      current_min_rtt_ = rtt;
    }
  }
  // We only need to check this once.
  if (rtt_sample_count_ == kHybridStartMinSamples) {
    // Divide min_rtt by 16 to get a rtt increase threshold for exiting.
    int min_rtt_increase_threshold_us = min_rtt.ToMicroseconds() >>
        kHybridStartDelayFactorExp;
    // Ensure the rtt threshold is never less than 2ms or more than 16ms.
    min_rtt_increase_threshold_us = min(min_rtt_increase_threshold_us,
                                        kHybridStartDelayMaxThresholdUs);
    QuicTime::Delta min_rtt_increase_threshold =
        QuicTime::Delta::FromMicroseconds(max(min_rtt_increase_threshold_us,
                                              kHybridStartDelayMinThresholdUs));

    found_delay_ = current_min_rtt_ > min_rtt.Add(min_rtt_increase_threshold);
  }
  // If either of the two conditions are met we exit from slow start.
  return found_ack_train_ || found_delay_;
}

}  // namespace net
