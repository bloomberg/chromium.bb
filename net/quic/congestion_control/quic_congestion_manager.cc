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
static const int kBitrateSmoothingPeriodMs = 1000;
static const int kHistoryPeriodMs = 5000;

static const int kDefaultRetransmissionTimeMs = 500;
// TCP RFC calls for 1 second RTO however Linux differs from this default and
// define the minimum RTO to 200ms, we will use the same until we have data to
// support a higher or lower value.
static const int kMinRetransmissionTimeMs = 200;
static const int kMaxRetransmissionTimeMs = 60000;
static const size_t kMaxRetransmissions = 10;
static const size_t kTailDropWindowSize = 5;
static const size_t kTailDropMaxRetransmissions = 4;

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
      largest_missing_(0),
      current_rtt_(QuicTime::Delta::Infinite()) {
}

QuicCongestionManager::~QuicCongestionManager() {
  STLDeleteValues(&packet_history_map_);
}

void QuicCongestionManager::SentPacket(QuicPacketSequenceNumber sequence_number,
                                       QuicTime sent_time,
                                       QuicByteCount bytes,
                                       Retransmission retransmission) {
  DCHECK(!ContainsKey(pending_packets_, sequence_number));
  send_algorithm_->SentPacket(sent_time, sequence_number, bytes,
                              retransmission);

  packet_history_map_[sequence_number] =
      new class SendAlgorithmInterface::SentPacket(bytes, sent_time);
  pending_packets_[sequence_number] = bytes;
  CleanupPacketHistory();
}

// Called when a packet is timed out.
void QuicCongestionManager::AbandoningPacket(
    QuicPacketSequenceNumber sequence_number) {
  PendingPacketsMap::iterator it = pending_packets_.find(sequence_number);
  if (it != pending_packets_.end()) {
    // Shouldn't this report loss as well? (decrease cgst window).
    send_algorithm_->AbandoningPacket(sequence_number, it->second);
    pending_packets_.erase(it);
  }
}

void QuicCongestionManager::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame, QuicTime feedback_receive_time) {
  send_algorithm_->OnIncomingQuicCongestionFeedbackFrame(
      frame, feedback_receive_time, packet_history_map_);
}

void QuicCongestionManager::OnIncomingAckFrame(const QuicAckFrame& frame,
                                               QuicTime ack_receive_time) {
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.find(frame.received_info.largest_observed);
  // TODO(satyamshekhar): largest_observed might be missing.
  if (history_it != packet_history_map_.end() &&
      !frame.received_info.delta_time_largest_observed.IsInfinite()) {
    QuicTime::Delta send_delta = ack_receive_time.Subtract(
        history_it->second->SendTimestamp());
    if (send_delta > frame.received_info.delta_time_largest_observed) {
      current_rtt_ = send_delta.Subtract(
          frame.received_info.delta_time_largest_observed);
    }
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
      send_algorithm_->OnIncomingAck(sequence_number, it->second, current_rtt_);
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

QuicTime::Delta QuicCongestionManager::TimeUntilSend(
    QuicTime now,
    Retransmission retransmission,
    HasRetransmittableData retransmittable,
    IsHandshake handshake) {
  return send_algorithm_->TimeUntilSend(now, retransmission, retransmittable,
                                        handshake);
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

const QuicTime::Delta QuicCongestionManager::rtt() {
  return current_rtt_;
}

const QuicTime::Delta QuicCongestionManager::DefaultRetransmissionTime() {
  return QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
}

const QuicTime::Delta QuicCongestionManager::GetRetransmissionDelay(
    size_t unacked_packets_count,
    size_t number_retransmissions) {
  QuicTime::Delta retransmission_delay = send_algorithm_->RetransmissionDelay();
  if (retransmission_delay.IsZero()) {
    // We are in the initial state, use default timeout values.
    if (unacked_packets_count <= kTailDropWindowSize) {
      if (number_retransmissions <= kTailDropMaxRetransmissions) {
        return QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
      }
      number_retransmissions -= kTailDropMaxRetransmissions;
    }
    retransmission_delay =
        QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  }
  // Calcluate exponential back off.
  retransmission_delay = QuicTime::Delta::FromMilliseconds(
      retransmission_delay.ToMilliseconds() * static_cast<size_t>(
          (1 << min<size_t>(number_retransmissions, kMaxRetransmissions))));

  // TODO(rch): This code should move to |send_algorithm_|.
  if (retransmission_delay.ToMilliseconds() < kMinRetransmissionTimeMs) {
    return QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs);
  }
  if (retransmission_delay.ToMilliseconds() > kMaxRetransmissionTimeMs) {
    return QuicTime::Delta::FromMilliseconds(kMaxRetransmissionTimeMs);
  }
  return retransmission_delay;
}

const QuicTime::Delta QuicCongestionManager::SmoothedRtt() {
  return send_algorithm_->SmoothedRtt();
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
