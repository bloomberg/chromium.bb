// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/quic_congestion_manager.h"

#include <algorithm>
#include <map>

#include "base/stl_util.h"
#include "net/quic/congestion_control/pacing_sender.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_protocol.h"

using std::map;
using std::min;

// A test-only flag to prevent the RTO from backing off when multiple sequential
// tail drops occur.
bool FLAGS_limit_rto_increase_for_tests = false;

// Do not remove this flag until the Finch-trials described in b/11706275
// are complete.
// If true, QUIC connections will support the use of a pacing algorithm when
// sending packets, in an attempt to reduce packet loss.  The client must also
// request pacing for the server to enable it.
bool FLAGS_enable_quic_pacing = false;

namespace net {
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

// We want to make sure if we get a nack packet which triggers several
// retransmissions, we don't queue up too many packets.  10 is TCP's default
// initial congestion window(RFC 6928).
static const size_t kMaxRetransmissionsPerAck = kDefaultInitialWindow;

// TCP retransmits after 3 nacks.
// TODO(ianswett): Change to match TCP's rule of retransmitting once an ack
// at least 3 sequence numbers larger arrives.
static const size_t kNumberOfNacksBeforeRetransmission = 3;

COMPILE_ASSERT(kHistoryPeriodMs >= kBitrateSmoothingPeriodMs,
               history_must_be_longer_or_equal_to_the_smoothing_period);
}  // namespace

QuicCongestionManager::QuicCongestionManager(
    const QuicClock* clock,
    CongestionFeedbackType congestion_type)
    : clock_(clock),
      send_algorithm_(SendAlgorithmInterface::Create(clock, congestion_type)),
      rtt_sample_(QuicTime::Delta::Infinite()),
      consecutive_rto_count_(0),
      using_pacing_(false) {
}

QuicCongestionManager::~QuicCongestionManager() {
  STLDeleteValues(&packet_history_map_);
}

void QuicCongestionManager::SetFromConfig(const QuicConfig& config,
                                          bool is_server) {
  if (config.initial_round_trip_time_us() > 0 &&
      rtt_sample_.IsInfinite()) {
    // The initial rtt should already be set on the client side.
    DVLOG_IF(1, !is_server)
        << "Client did not set an initial RTT, but did negotiate one.";
    rtt_sample_ =
        QuicTime::Delta::FromMicroseconds(config.initial_round_trip_time_us());
  }
  if (config.congestion_control() == kPACE) {
    MaybeEnablePacing();
  }
  send_algorithm_->SetFromConfig(config, is_server);
}

void QuicCongestionManager::SetMaxPacketSize(QuicByteCount max_packet_size) {
  send_algorithm_->SetMaxPacketSize(max_packet_size);
}

void QuicCongestionManager::OnPacketSent(
    QuicPacketSequenceNumber sequence_number,
    QuicTime sent_time,
    QuicByteCount bytes,
    TransmissionType transmission_type,
    HasRetransmittableData has_retransmittable_data) {
  DCHECK_LT(0u, sequence_number);
  DCHECK(!ContainsKey(pending_packets_, sequence_number));

  // Only track packets the send algorithm wants us to track.
  if (!send_algorithm_->OnPacketSent(sent_time, sequence_number, bytes,
                                     transmission_type,
                                     has_retransmittable_data)) {
    return;
  }
  packet_history_map_[sequence_number] = new SendAlgorithmInterface::SentPacket(
      bytes, sent_time, has_retransmittable_data);
  pending_packets_.insert(sequence_number);
  CleanupPacketHistory();
}

void QuicCongestionManager::OnRetransmissionTimeout() {
  ++consecutive_rto_count_;
  send_algorithm_->OnRetransmissionTimeout();
  // Abandon all pending packets to ensure the congestion window
  // opens up before we attempt to retransmit packets.
  for (SequenceNumberSet::const_iterator it = pending_packets_.begin();
       it != pending_packets_.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = *it;
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    send_algorithm_->OnPacketAbandoned(
        sequence_number, packet_history_map_[sequence_number]->bytes_sent());
  }
  pending_packets_.clear();
}

void QuicCongestionManager::OnLeastUnackedIncreased() {
  consecutive_rto_count_ = 0;
}

void QuicCongestionManager::OnPacketAbandoned(
    QuicPacketSequenceNumber sequence_number) {
  SequenceNumberSet::iterator it = pending_packets_.find(sequence_number);
  if (it != pending_packets_.end()) {
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    send_algorithm_->OnPacketAbandoned(
        sequence_number, packet_history_map_[sequence_number]->bytes_sent());
    pending_packets_.erase(it);
  }
}

void QuicCongestionManager::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame, QuicTime feedback_receive_time) {
  send_algorithm_->OnIncomingQuicCongestionFeedbackFrame(
      frame, feedback_receive_time, packet_history_map_);
}

SequenceNumberSet QuicCongestionManager::OnIncomingAckFrame(
    const QuicAckFrame& frame,
    QuicTime ack_receive_time) {
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.find(frame.received_info.largest_observed);
  // TODO(satyamshekhar): largest_observed might be missing.
  if (history_it != packet_history_map_.end()) {
    QuicTime::Delta send_delta = ack_receive_time.Subtract(
        history_it->second->send_timestamp());
    if (send_delta > frame.received_info.delta_time_largest_observed) {
      rtt_sample_ = send_delta.Subtract(
          frame.received_info.delta_time_largest_observed);
    } else if (rtt_sample_.IsInfinite()) {
      // Even though we received information from the peer suggesting
      // an invalid (negative) RTT, we can use the send delta as an
      // approximation until we get a better estimate.
      rtt_sample_ = send_delta;
    }
  }
  // We want to.
  // * Get all packets lower(including) than largest_observed
  //   from pending_packets_.
  // * Remove all missing packets.
  // * Send each ACK in the list to send_algorithm_.
  SequenceNumberSet::iterator it = pending_packets_.begin();
  SequenceNumberSet::iterator it_upper =
      pending_packets_.upper_bound(frame.received_info.largest_observed);

  SequenceNumberSet retransmission_packets;
  SequenceNumberSet lost_packets;
  while (it != it_upper) {
    QuicPacketSequenceNumber sequence_number = *it;
    if (!IsAwaitingPacket(frame.received_info, sequence_number)) {
      // Not missing, hence implicitly acked.
      size_t bytes_sent = packet_history_map_[sequence_number]->bytes_sent();
      send_algorithm_->OnPacketAcked(sequence_number, bytes_sent, rtt_sample_);
      pending_packets_.erase(it++);  // Must be incremented post to work.
      continue;
    }

    // The peer got packets after this sequence number.  This is an explicit
    // nack.
    DVLOG(1) << "still missing packet " << sequence_number;
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    const SendAlgorithmInterface::SentPacket* sent_packet =
        packet_history_map_[sequence_number];
    // Consider it multiple nacks when there is a gap between the missing packet
    // and the largest observed, since the purpose of a nack threshold is to
    // tolerate re-ordering.  This handles both StretchAcks and Forward Acks.
    // TODO(ianswett): This relies heavily on sequential reception of packets,
    // and makes an assumption that the congestion control uses TCP style nacks.
    size_t min_nacks = frame.received_info.largest_observed - sequence_number;
    packet_history_map_[sequence_number]->Nack(min_nacks);

    size_t num_nacks_needed = kNumberOfNacksBeforeRetransmission;
    // Check for early retransmit(RFC5827) when the last packet gets acked and
    // the there are fewer than 4 pending packets.
    if (pending_packets_.size() <= kNumberOfNacksBeforeRetransmission &&
        sent_packet->has_retransmittable_data() == HAS_RETRANSMITTABLE_DATA &&
        *pending_packets_.rbegin() == frame.received_info.largest_observed) {
      num_nacks_needed = frame.received_info.largest_observed - sequence_number;
    }

    if (sent_packet->nack_count() < num_nacks_needed) {
      ++it;
      continue;
    }

    // If the number of retransmissions has maxed out, don't lose or retransmit
    // any more packets.
    if (retransmission_packets.size() >= kMaxRetransmissionsPerAck) {
      ++it;
      continue;
    }

    lost_packets.insert(sequence_number);
    if (sent_packet->has_retransmittable_data() == HAS_RETRANSMITTABLE_DATA) {
      retransmission_packets.insert(sequence_number);
    }

    ++it;
  }
  // Abandon packets after the loop over pending packets, because otherwise it
  // changes the early retransmit logic and iteration.
  for (SequenceNumberSet::const_iterator it = lost_packets.begin();
       it != lost_packets.end(); ++it) {
    // TODO(ianswett): OnPacketLost is also called from TCPCubicSender when
    // an FEC packet is lost, but FEC loss information should be shared among
    // congestion managers.  Additionally, if it's expected the FEC packet may
    // repair the loss, it should be recorded as a loss to the congestion
    // manager, but not retransmitted until it's known whether the FEC packet
    // arrived.
    send_algorithm_->OnPacketLost(*it, ack_receive_time);
    OnPacketAbandoned(*it);
  }

  return retransmission_packets;
}

QuicTime::Delta QuicCongestionManager::TimeUntilSend(
    QuicTime now,
    TransmissionType transmission_type,
    HasRetransmittableData retransmittable,
    IsHandshake handshake) {
  return send_algorithm_->TimeUntilSend(now, transmission_type, retransmittable,
                                        handshake);
}

const QuicTime::Delta QuicCongestionManager::DefaultRetransmissionTime() {
  return QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
}

// Ensures that the Delayed Ack timer is always set to a value lesser
// than the retransmission timer's minimum value (MinRTO). We want the
// delayed ack to get back to the QUIC peer before the sender's
// retransmission timer triggers.  Since we do not know the
// reverse-path one-way delay, we assume equal delays for forward and
// reverse paths, and ensure that the timer is set to less than half
// of the MinRTO.
// There may be a value in making this delay adaptive with the help of
// the sender and a signaling mechanism -- if the sender uses a
// different MinRTO, we may get spurious retransmissions. May not have
// any benefits, but if the delayed ack becomes a significant source
// of (likely, tail) latency, then consider such a mechanism.

const QuicTime::Delta QuicCongestionManager::DelayedAckTime() {
  return QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs/2);
}

const QuicTime::Delta QuicCongestionManager::GetRetransmissionDelay() const {
  size_t number_retransmissions = consecutive_rto_count_;
  if (FLAGS_limit_rto_increase_for_tests) {
    const size_t kTailDropWindowSize = 5;
    const size_t kTailDropMaxRetransmissions = 4;
    if (pending_packets_.size() <= kTailDropWindowSize) {
      // Avoid exponential backoff of RTO when there are only a few packets
      // outstanding.  This helps avoid the situation where fake packet loss
      // causes a packet and it's retransmission to be dropped causing
      // test timouts.
      if (number_retransmissions <= kTailDropMaxRetransmissions) {
        number_retransmissions = 0;
      } else {
        number_retransmissions -= kTailDropMaxRetransmissions;
      }
    }
  }

  QuicTime::Delta retransmission_delay = send_algorithm_->RetransmissionDelay();
  if (retransmission_delay.IsZero()) {
    // We are in the initial state, use default timeout values.
    retransmission_delay =
        QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  }
  // Calculate exponential back off.
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

const QuicTime::Delta QuicCongestionManager::SmoothedRtt() const {
  return send_algorithm_->SmoothedRtt();
}

QuicBandwidth QuicCongestionManager::BandwidthEstimate() const {
  return send_algorithm_->BandwidthEstimate();
}

QuicByteCount QuicCongestionManager::GetCongestionWindow() const {
  return send_algorithm_->GetCongestionWindow();
}

void QuicCongestionManager::SetCongestionWindow(QuicByteCount window) {
  send_algorithm_->SetCongestionWindow(window);
}

void QuicCongestionManager::CleanupPacketHistory() {
  const QuicTime::Delta kHistoryPeriod =
      QuicTime::Delta::FromMilliseconds(kHistoryPeriodMs);
  QuicTime now = clock_->ApproximateNow();

  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.begin();
  for (; history_it != packet_history_map_.end(); ++history_it) {
    if (now.Subtract(history_it->second->send_timestamp()) <= kHistoryPeriod) {
      return;
    }
    // Don't remove packets which have not been acked.
    if (ContainsKey(pending_packets_, history_it->first)) {
      continue;
    }
    delete history_it->second;
    packet_history_map_.erase(history_it);
    history_it = packet_history_map_.begin();
  }
}

void QuicCongestionManager::MaybeEnablePacing() {
  if (!FLAGS_enable_quic_pacing) {
    return;
  }

  if (using_pacing_) {
    return;
  }

  using_pacing_ = true;
  send_algorithm_.reset(
      new PacingSender(send_algorithm_.release(),
                       QuicTime::Delta::FromMicroseconds(1)));
}

}  // namespace net
