// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/quic_congestion_manager.h"

#include <algorithm>
#include <map>

#include "base/stl_util.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"

namespace {
const int kBitrateSmoothingPeriodMs = 3000;
const int kMinBitrateSmoothingPeriodMs = 500;
const int kHistoryPeriodMs = 5000;

const int kDefaultRetransmissionTimeMs = 500;
const size_t kMaxRetransmissions = 15;

COMPILE_ASSERT(kHistoryPeriodMs >= kBitrateSmoothingPeriodMs,
               history_must_be_longer_or_equal_to_the_smoothing_period);
}  // namespace

using std::map;
using std::min;

namespace net {

QuicCongestionManager::QuicCongestionManager(
    const QuicClock* clock,
    CongestionFeedbackType type)
    : clock_(clock),
      receive_algorithm_(ReceiveAlgorithmInterface::Create(clock, type)),
      send_algorithm_(SendAlgorithmInterface::Create(clock, type)) {
}

QuicCongestionManager::~QuicCongestionManager() {
  STLDeleteValues(&packet_history_map_);
}

void QuicCongestionManager::SentPacket(QuicPacketSequenceNumber sequence_number,
                                       QuicByteCount bytes,
                                       bool is_retransmission) {
  DCHECK(!ContainsKey(pending_packets_, sequence_number));
  send_algorithm_->SentPacket(sequence_number, bytes, is_retransmission);

  packet_history_map_[sequence_number] =
      new class SendAlgorithmInterface::SentPacket(bytes, clock_->Now());
  pending_packets_[sequence_number] = bytes;
  CleanupPacketHistory();
  DLOG(INFO) << "Sent sequence number:" << sequence_number;
}

void QuicCongestionManager::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame) {
  send_algorithm_->OnIncomingQuicCongestionFeedbackFrame(frame,
                                                         packet_history_map_);
}

void QuicCongestionManager::OnIncomingAckFrame(const QuicAckFrame& frame) {
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  QuicTime::Delta rtt = QuicTime::Delta::Zero();
  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.find(frame.received_info.largest_observed);
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
  it_upper = pending_packets_.upper_bound(frame.received_info.largest_observed);

  while (it != it_upper) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (!frame.received_info.IsAwaitingPacket(sequence_number)) {
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

QuicTime::Delta QuicCongestionManager::TimeUntilSend(bool is_retransmission) {
  return send_algorithm_->TimeUntilSend(is_retransmission);
}

bool QuicCongestionManager::GenerateCongestionFeedback(
    QuicCongestionFeedbackFrame* feedback) {
  return receive_algorithm_->GenerateCongestionFeedback(feedback);
}

void QuicCongestionManager::RecordIncomingPacket(
    QuicByteCount bytes,
    QuicPacketSequenceNumber sequence_number,
    QuicTime timestamp,
    bool revived) {
  receive_algorithm_->RecordIncomingPacket(bytes, sequence_number, timestamp,
                                           revived);
}

// static
const QuicTime::Delta QuicCongestionManager::DefaultRetransmissionTime() {
  return QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
}

// static
const QuicTime::Delta QuicCongestionManager::GetRetransmissionDelay(
    size_t number_retransmissions) {
  return QuicTime::Delta::FromMilliseconds(
      kDefaultRetransmissionTimeMs *
      (1 << min<size_t>(number_retransmissions, kMaxRetransmissions)));
}

QuicBandwidth QuicCongestionManager::SentBandwidth() const {
  const QuicTime::Delta kBitrateSmoothingPeriod =
      QuicTime::Delta::FromMilliseconds(kBitrateSmoothingPeriodMs);
  const QuicTime::Delta kMinBitrateSmoothingPeriod =
      QuicTime::Delta::FromMilliseconds(kMinBitrateSmoothingPeriodMs);

  QuicTime now = clock_->Now();
  QuicByteCount sum_bytes_sent = 0;

  // Sum packet from new until they are kBitrateSmoothingPeriod old.
  SendAlgorithmInterface::SentPacketsMap::const_reverse_iterator history_rit =
      packet_history_map_.rbegin();

  QuicTime::Delta max_diff = QuicTime::Delta::Zero();
  for (; history_rit != packet_history_map_.rend(); ++history_rit) {
    QuicTime::Delta diff = now.Subtract(history_rit->second->SendTimestamp());
    if (diff > kBitrateSmoothingPeriod) {
      break;
    }
    sum_bytes_sent += history_rit->second->BytesSent();
    max_diff = diff;
  }
  if (max_diff < kMinBitrateSmoothingPeriod) {
    // No estimate.
    return QuicBandwidth::Zero();
  }
  return QuicBandwidth::FromBytesAndTimeDelta(sum_bytes_sent, max_diff);
}

QuicBandwidth QuicCongestionManager::BandwidthEstimate() {
  return send_algorithm_->BandwidthEstimate();
}

void QuicCongestionManager::CleanupPacketHistory() {
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

}  // namespace net
