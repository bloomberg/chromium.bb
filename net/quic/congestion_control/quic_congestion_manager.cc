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
const int kBitrateSmoothingPeriodMs = 1000;
const int kMinBitrateSmoothingPeriodMs = 500;
const int kHistoryPeriodMs = 5000;

const int kDefaultRetransmissionTimeMs = 500;
const size_t kMaxRetransmissions = 10;
const size_t kTailDropWindowSize = 5;

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
      send_algorithm_(SendAlgorithmInterface::Create(clock, type)),
      largest_missing_(0) {
}

QuicCongestionManager::~QuicCongestionManager() {
  STLDeleteValues(&packet_history_map_);
}

void QuicCongestionManager::SentPacket(QuicPacketSequenceNumber sequence_number,
                                       QuicTime sent_time,
                                       QuicByteCount bytes,
                                       bool is_retransmission) {
  DCHECK(!ContainsKey(pending_packets_, sequence_number));
  send_algorithm_->SentPacket(sent_time, sequence_number, bytes,
                              is_retransmission);

  packet_history_map_[sequence_number] =
      new class SendAlgorithmInterface::SentPacket(bytes, sent_time);
  pending_packets_[sequence_number] = bytes;
  CleanupPacketHistory();
}

void QuicCongestionManager::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame, QuicTime feedback_receive_time) {
  QuicBandwidth sent_bandwidth = SentBandwidth(feedback_receive_time);
  send_algorithm_->OnIncomingQuicCongestionFeedbackFrame(
      frame, feedback_receive_time, sent_bandwidth, packet_history_map_);
}

void QuicCongestionManager::OnIncomingAckFrame(const QuicAckFrame& frame,
                                               QuicTime ack_receive_time) {
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  QuicTime::Delta rtt = QuicTime::Delta::Zero();
  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.find(frame.received_info.largest_observed);
  if (history_it != packet_history_map_.end()) {
    // TODO(pwestin): we need to add the delta to the feedback message.
    rtt = ack_receive_time.Subtract(history_it->second->SendTimestamp());
  }
  // We want to.
  // * Get all packets lower(including) than largest_observed
  //   from pending_packets_.
  // * Remove all missing packets.
  // * Send each ACK in the list to send_algorithm_.
  PendingPacketsMap::iterator it, it_upper;
  it = pending_packets_.begin();
  it_upper = pending_packets_.upper_bound(frame.received_info.largest_observed);

  bool new_packet_loss_reported = false;
  while (it != it_upper) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (!IsAwaitingPacket(frame.received_info, sequence_number)) {
      // Not missing, hence implicitly acked.
      send_algorithm_->OnIncomingAck(sequence_number,
                                     it->second,
                                     rtt);
      pending_packets_.erase(it++);  // Must be incremented post to work.
    } else {
      if (sequence_number > largest_missing_) {
        // We have a new loss reported.
        new_packet_loss_reported = true;
        largest_missing_ = sequence_number;
      }
      ++it;
    }
  }
  if (new_packet_loss_reported) {
    send_algorithm_->OnIncomingLoss(ack_receive_time);
  }
}

QuicTime::Delta QuicCongestionManager::TimeUntilSend(QuicTime now,
                                                     bool is_retransmission) {
  return send_algorithm_->TimeUntilSend(now, is_retransmission);
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
    size_t unacked_packets_count,
    size_t number_retransmissions) {
  if (unacked_packets_count <= kTailDropWindowSize) {
    return QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  }

  return QuicTime::Delta::FromMilliseconds(
      kDefaultRetransmissionTimeMs *
      (1 << min<size_t>(number_retransmissions, kMaxRetransmissions)));
}

QuicBandwidth QuicCongestionManager::SentBandwidth(
    QuicTime feedback_receive_time) const {
  const QuicTime::Delta kBitrateSmoothingPeriod =
      QuicTime::Delta::FromMilliseconds(kBitrateSmoothingPeriodMs);
  const QuicTime::Delta kMinBitrateSmoothingPeriod =
      QuicTime::Delta::FromMilliseconds(kMinBitrateSmoothingPeriodMs);

  QuicByteCount sum_bytes_sent = 0;

  // Sum packet from new until they are kBitrateSmoothingPeriod old.
  SendAlgorithmInterface::SentPacketsMap::const_reverse_iterator history_rit =
      packet_history_map_.rbegin();

  QuicTime::Delta max_diff = QuicTime::Delta::Zero();
  for (; history_rit != packet_history_map_.rend(); ++history_rit) {
    QuicTime::Delta diff =
        feedback_receive_time.Subtract(history_rit->second->SendTimestamp());
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
  QuicTime now = clock_->ApproximateNow();

  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.begin();
  for (; history_it != packet_history_map_.end(); ++history_it) {
    if (now.Subtract(history_it->second->SendTimestamp()) <= kHistoryPeriod) {
      return;
    }
    delete history_it->second;
    packet_history_map_.erase(history_it);
    history_it = packet_history_map_.begin();
  }
}

}  // namespace net
