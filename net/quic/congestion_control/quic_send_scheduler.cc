// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "net/quic/congestion_control/quic_send_scheduler.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "base/stl_util.h"
#include "base/time.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
//#include "util/gtl/map-util.h"

namespace {
const int kBitrateSmoothingPeriodMs = 3000;
const int kMinBitrateSmoothingPeriodMs = 500;
const int kHistoryPeriodMs = 5000;

COMPILE_ASSERT(kHistoryPeriodMs >= kBitrateSmoothingPeriodMs,
               history_must_be_longer_or_equal_to_the_smoothing_period);
}

using std::map;
using std::max;

namespace net {

const int64 kNumMicrosPerSecond = base::Time::kMicrosecondsPerSecond;

QuicSendScheduler::QuicSendScheduler(
    const QuicClock* clock,
    CongestionFeedbackType type)
    : clock_(clock),
      current_estimated_bandwidth_(0),
      max_estimated_bandwidth_(-1),
      last_sent_packet_(QuicTime::FromMicroseconds(0)),
      send_algorithm_(SendAlgorithmInterface::Create(clock, type)) {
}

QuicSendScheduler::~QuicSendScheduler() {
  STLDeleteValues(&packet_history_map_);
}

void QuicSendScheduler::SentPacket(QuicPacketSequenceNumber sequence_number,
                                   size_t bytes,
                                   bool is_retransmission) {
  DCHECK(!ContainsKey(pending_packets_, sequence_number));
  send_algorithm_->SentPacket(sequence_number, bytes, is_retransmission);

  packet_history_map_[sequence_number] =
      new class SendAlgorithmInterface::SentPacket(bytes, clock_->Now());
  pending_packets_[sequence_number] = bytes;
  CleanupPacketHistory();
  DLOG(INFO) << "Sent sequence number:" << sequence_number;
}

void QuicSendScheduler::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& congestion_feedback_frame) {
  send_algorithm_->OnIncomingQuicCongestionFeedbackFrame(
      congestion_feedback_frame, packet_history_map_);
}

void QuicSendScheduler::CleanupPacketHistory() {
  const QuicTime::Delta kHistoryPeriod =
      QuicTime::Delta::FromMilliseconds(kHistoryPeriodMs);
  QuicTime Now = clock_->Now();

  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.begin();
  for (; history_it != packet_history_map_.end(); ++history_it) {
    if (Now.Subtract(history_it->second->SendTimestamp()) <= kHistoryPeriod) {
      return;
    }
    DLOG(INFO) << "Clear sequence number:" << history_it->first
               << "from history";
    delete history_it->second;
    packet_history_map_.erase(history_it);
    history_it = packet_history_map_.begin();
  }
}

void QuicSendScheduler::OnIncomingAckFrame(const QuicAckFrame& ack_frame) {
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  QuicTime::Delta rtt = QuicTime::Delta::Zero();
  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.find(ack_frame.received_info.largest_observed);
  if (history_it != packet_history_map_.end()) {
    rtt = clock_->Now().Subtract(history_it->second->SendTimestamp());
  }
  // We want to.
  // * Get all packets lower(including) than largest_observed
  //   from pending_packets_.
  // * Remove all missing packets.
  // * Send each ACK in the list to send_algorithm_.
  PendingPacketsMap::iterator it, it_upper;
  it = pending_packets_.begin();
  it_upper = pending_packets_.upper_bound(
      ack_frame.received_info.largest_observed);

  while (it != it_upper) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (!ack_frame.received_info.IsAwaitingPacket(sequence_number)) {
      // Not missing, hence implicitly acked.
      send_algorithm_->OnIncomingAck(sequence_number,
                                     it->second,
                                     rtt);
      DLOG(INFO) << "ACKed sequence number:" << sequence_number;
      pending_packets_.erase(it++);  // Must be incremented post to work.
    } else {
      ++it;
    }
  }
}

QuicTime::Delta QuicSendScheduler::TimeUntilSend(bool is_retransmission) {
  return send_algorithm_->TimeUntilSend(is_retransmission);
}

size_t QuicSendScheduler::AvailableCongestionWindow() {
  return send_algorithm_->AvailableCongestionWindow();
}

int QuicSendScheduler::BandwidthEstimate() {
  int bandwidth_estimate = send_algorithm_->BandwidthEstimate();
  if (bandwidth_estimate == kNoValidEstimate) {
    // If we don't have a valid estimate use the send rate.
    return SentBandwidth();
  }
  return bandwidth_estimate;
}

// TODO(pwestin): add a timer to make max_estimated_bandwidth_ accurate.
int QuicSendScheduler::SentBandwidth() {
  const QuicTime::Delta kBitrateSmoothingPeriod =
      QuicTime::Delta::FromMilliseconds(kBitrateSmoothingPeriodMs);
  const QuicTime::Delta kMinBitrateSmoothingPeriod =
      QuicTime::Delta::FromMilliseconds(kMinBitrateSmoothingPeriodMs);

  QuicTime Now = clock_->Now();
  size_t sum_bytes_sent = 0;

  // Sum packet from new until they are kBitrateSmoothingPeriod old.
  SendAlgorithmInterface::SentPacketsMap::reverse_iterator history_rit =
      packet_history_map_.rbegin();

  QuicTime::Delta max_diff = QuicTime::Delta::Zero();
  for (; history_rit != packet_history_map_.rend(); ++history_rit) {
    QuicTime::Delta diff = Now.Subtract(history_rit->second->SendTimestamp());
    if (diff > kBitrateSmoothingPeriod) {
      break;
    }
    sum_bytes_sent += history_rit->second->BytesSent();
    max_diff = diff;
  }
  if (max_diff < kMinBitrateSmoothingPeriod) {
    // No estimate.
    return 0;
  }
  current_estimated_bandwidth_ = sum_bytes_sent * kNumMicrosPerSecond /
      max_diff.ToMicroseconds();
  max_estimated_bandwidth_ = max(max_estimated_bandwidth_,
                                 current_estimated_bandwidth_);
  return current_estimated_bandwidth_;
}

int QuicSendScheduler::PeakSustainedBandwidth() {
  // To make sure that we get the latest estimate we call SentBandwidth.
    SentBandwidth();
  return max_estimated_bandwidth_;
}

}  // namespace net
