// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/congestion_control/pacing_sender.h"
#include "net/quic/quic_ack_notifier_manager.h"

using std::make_pair;
using std::min;

// TODO(rtenneti): Remove this.
// Do not flip this flag until the flakiness of the
// net/tools/quic/end_to_end_test is fixed.
// If true, then QUIC connections will track the retransmission history of a
// packet so that an ack of a previous transmission will ack the data of all
// other transmissions.
bool FLAGS_track_retransmission_history = false;

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

// We only retransmit 2 packets per ack.
static const size_t kMaxRetransmissionsPerAck = 2;

// TCP retransmits after 3 nacks.
static const size_t kNumberOfNacksBeforeRetransmission = 3;

COMPILE_ASSERT(kHistoryPeriodMs >= kBitrateSmoothingPeriodMs,
               history_must_be_longer_or_equal_to_the_smoothing_period);
}  // namespace

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicSentPacketManager::HelperInterface::~HelperInterface() {
}

QuicSentPacketManager::QuicSentPacketManager(bool is_server,
                                             HelperInterface* helper,
                                             const QuicClock* clock,
                                             CongestionFeedbackType type)
    : is_server_(is_server),
      helper_(helper),
      clock_(clock),
      send_algorithm_(SendAlgorithmInterface::Create(clock, type)),
      rtt_sample_(QuicTime::Delta::Infinite()),
      consecutive_rto_count_(0),
      using_pacing_(false) {
}

QuicSentPacketManager::~QuicSentPacketManager() {
  for (UnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    delete it->second.retransmittable_frames;
    // Only delete previous_transmissions once, for the newest packet.
    if (it->second.previous_transmissions != NULL &&
        it->first == *it->second.previous_transmissions->rbegin()) {
      delete it->second.previous_transmissions;
    }
  }
  STLDeleteValues(&packet_history_map_);
}

void QuicSentPacketManager::SetFromConfig(const QuicConfig& config) {
  if (config.initial_round_trip_time_us() > 0 &&
      rtt_sample_.IsInfinite()) {
    // The initial rtt should already be set on the client side.
    DVLOG_IF(1, !is_server_)
        << "Client did not set an initial RTT, but did negotiate one.";
    rtt_sample_ =
        QuicTime::Delta::FromMicroseconds(config.initial_round_trip_time_us());
  }
  if (config.congestion_control() == kPACE) {
    MaybeEnablePacing();
  }
  send_algorithm_->SetFromConfig(config, is_server_);
}

void QuicSentPacketManager::SetMaxPacketSize(QuicByteCount max_packet_size) {
  send_algorithm_->SetMaxPacketSize(max_packet_size);
}

// TODO(ianswett): Combine this method with OnPacketSent once packets are always
// sent in order and the connection tracks RetransmittableFrames for longer.
void QuicSentPacketManager::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  if (serialized_packet.retransmittable_frames) {
    ack_notifier_manager_.OnSerializedPacket(serialized_packet);
  }

  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < serialized_packet.sequence_number);
  unacked_packets_[serialized_packet.sequence_number] =
      TransmissionInfo(serialized_packet.retransmittable_frames,
                       serialized_packet.sequence_number_length);
}

void QuicSentPacketManager::OnRetransmittedPacket(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number) {
  DCHECK(ContainsKey(unacked_packets_, old_sequence_number));
  DCHECK(ContainsKey(pending_retransmissions_, old_sequence_number));
  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < new_sequence_number);

  pending_retransmissions_.erase(old_sequence_number);
  // TODO(ianswett): Discard and lose the packet lazily instead of immediately.

  UnackedPacketMap::iterator unacked_it =
      unacked_packets_.find(old_sequence_number);
  RetransmittableFrames* frames = unacked_it->second.retransmittable_frames;
  DCHECK(frames);

  // A notifier may be waiting to hear about ACKs for the original sequence
  // number. Inform them that the sequence number has changed.
  ack_notifier_manager_.UpdateSequenceNumber(old_sequence_number,
                                             new_sequence_number);

  // We keep the old packet in the unacked packet list until it, or one of
  // the retransmissions of it are acked.
  unacked_it->second.retransmittable_frames = NULL;
  unacked_packets_[new_sequence_number] =
      TransmissionInfo(frames, GetSequenceNumberLength(old_sequence_number));

  // Keep track of all sequence numbers that this packet
  // has been transmitted as.
  SequenceNumberSet* previous_transmissions =
      unacked_it->second.previous_transmissions;
  if (previous_transmissions == NULL) {
    // This is the first retransmission of this packet, so create a new entry.
    previous_transmissions = new SequenceNumberSet;
    unacked_it->second.previous_transmissions = previous_transmissions;
    previous_transmissions->insert(old_sequence_number);
  }
  previous_transmissions->insert(new_sequence_number);
  unacked_packets_[new_sequence_number].previous_transmissions =
      previous_transmissions;

  DCHECK(HasRetransmittableFrames(new_sequence_number));
}

bool QuicSentPacketManager::OnIncomingAck(
    const ReceivedPacketInfo& received_info, QuicTime ack_receive_time) {
  // Determine if the least unacked sequence number is being acked.
  QuicPacketSequenceNumber least_unacked_sent_before =
      GetLeastUnackedSentPacket();
  bool new_least_unacked = !IsAwaitingPacket(received_info,
                                             least_unacked_sent_before);

  MaybeUpdateRTT(received_info, ack_receive_time);
  HandleAckForSentPackets(received_info);
  MaybeRetransmitOnAckFrame(received_info, ack_receive_time);

  if (new_least_unacked) {
    consecutive_rto_count_ = 0;
  }

  return new_least_unacked;
}

void QuicSentPacketManager::DiscardUnackedPacket(
    QuicPacketSequenceNumber sequence_number) {
  MarkPacketHandled(sequence_number, NOT_RECEIVED_BY_PEER);
}

void QuicSentPacketManager::HandleAckForSentPackets(
    const ReceivedPacketInfo& received_info) {
  // Go through the packets we have not received an ack for and see if this
  // incoming_ack shows they've been seen by the peer.
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number > received_info.largest_observed) {
      // These are very new sequence_numbers.
      break;
    }

    if (IsAwaitingPacket(received_info, sequence_number)) {
      ++it;
      continue;
    }

    // Packet was acked, so remove it from our unacked packet list.
    DVLOG(1) << ENDPOINT <<"Got an ack for packet " << sequence_number;
    // If data is associated with the most recent transmission of this
    // packet, then inform the caller.
    it = MarkPacketHandled(sequence_number, RECEIVED_BY_PEER);

    // The AckNotifierManager is informed of every ACKed sequence number.
    ack_notifier_manager_.OnPacketAcked(sequence_number);
  }

  // If we have received a truncated ack, then we need to
  // clear out some previous transmissions to allow the peer
  // to actually ACK new packets.
  if (received_info.is_truncated) {
    ClearPreviousRetransmissions(received_info.missing_packets.size() / 2);
  }
}

void QuicSentPacketManager::ClearPreviousRetransmissions(size_t num_to_clear) {
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end() && num_to_clear > 0) {
    QuicPacketSequenceNumber sequence_number = it->first;
    // If this is not a previous transmission then there is no point
    // in clearing out any further packets, because it will not affect
    // the high water mark.
    SequenceNumberSet* previous_transmissions =
        it->second.previous_transmissions;
    if (previous_transmissions == NULL) {
      break;
    }
    QuicPacketSequenceNumber newest_transmission =
        *previous_transmissions->rbegin();
    if (sequence_number == newest_transmission) {
      break;
    }

    DCHECK(it->second.retransmittable_frames == NULL);
    previous_transmissions->erase(sequence_number);
    if (previous_transmissions->size() == 1) {
      unacked_packets_[newest_transmission].previous_transmissions = NULL;
      delete previous_transmissions;
    }
    unacked_packets_.erase(it++);
    --num_to_clear;
  }
}

bool QuicSentPacketManager::HasRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  if (!ContainsKey(unacked_packets_, sequence_number)) {
    return false;
  }

  return unacked_packets_.find(
      sequence_number)->second.retransmittable_frames != NULL;
}

void QuicSentPacketManager::RetransmitUnackedPackets(
    RetransmissionType retransmission_type) {
  if (unacked_packets_.empty()) {
    return;
  }

  for (UnackedPacketMap::const_iterator unacked_it = unacked_packets_.begin();
       unacked_it != unacked_packets_.end(); ++unacked_it) {
    const RetransmittableFrames* frames =
        unacked_it->second.retransmittable_frames;
    if (frames == NULL) {
      continue;
    }
    if (retransmission_type == ALL_PACKETS ||
        frames->encryption_level() == ENCRYPTION_INITIAL) {
      // TODO(satyamshekhar): Think about congestion control here.
      // Specifically, about the retransmission count of packets being sent
      // proactively to achieve 0 (minimal) RTT.
      if (unacked_it->second.retransmittable_frames) {
        OnPacketAbandoned(unacked_it->first);
        MarkForRetransmission(unacked_it->first, NACK_RETRANSMISSION);
      } else {
        DiscardUnackedPacket(unacked_it->first);
      }
    }
  }
}

void QuicSentPacketManager::MarkForRetransmission(
    QuicPacketSequenceNumber sequence_number,
    TransmissionType transmission_type) {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));
  DCHECK(HasRetransmittableFrames(sequence_number));
  // TODO(ianswett): Currently the RTO can fire while there are pending NACK
  // retransmissions for the same data, which is not ideal.
  if (ContainsKey(pending_retransmissions_, sequence_number)) {
    return;
  }

  pending_retransmissions_[sequence_number] = transmission_type;
}

bool QuicSentPacketManager::HasPendingRetransmissions() const {
  return !pending_retransmissions_.empty();
}

QuicSentPacketManager::PendingRetransmission
    QuicSentPacketManager::NextPendingRetransmission() {
  DCHECK(!pending_retransmissions_.empty());
  QuicPacketSequenceNumber sequence_number =
      pending_retransmissions_.begin()->first;
  DCHECK(ContainsKey(unacked_packets_, sequence_number));
  const RetransmittableFrames* retransmittable_frames =
      unacked_packets_[sequence_number].retransmittable_frames;
  DCHECK(retransmittable_frames);

  return PendingRetransmission(sequence_number,
                               pending_retransmissions_.begin()->second,
                               *retransmittable_frames,
                               GetSequenceNumberLength(sequence_number));
}

bool QuicSentPacketManager::IsPreviousTransmission(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  UnackedPacketMap::const_iterator it = unacked_packets_.find(sequence_number);
  if (it->second.previous_transmissions == NULL) {
    return false;
  }

  SequenceNumberSet* previous_transmissions = it->second.previous_transmissions;
  DCHECK(!previous_transmissions->empty());
  return *previous_transmissions->rbegin() != sequence_number;
}

QuicSentPacketManager::UnackedPacketMap::iterator
QuicSentPacketManager::MarkPacketHandled(
    QuicPacketSequenceNumber sequence_number, ReceivedByPeer received_by_peer) {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  // If this packet is pending, remove it and inform the send algorithm.
  if (pending_packets_.erase(sequence_number)) {
    size_t bytes_sent = packet_history_map_[sequence_number]->bytes_sent();
    if (received_by_peer == RECEIVED_BY_PEER) {
      send_algorithm_->OnPacketAcked(sequence_number, bytes_sent, rtt_sample_);
    } else {
      // It's been abandoned.
      send_algorithm_->OnPacketAbandoned(sequence_number, bytes_sent);
    }
  }

  // If this packet has never been retransmitted, then simply drop it.
  UnackedPacketMap::const_iterator previous_it =
      unacked_packets_.find(sequence_number);
  if (previous_it->second.previous_transmissions == NULL) {
    UnackedPacketMap::iterator next_unacked =
        unacked_packets_.find(sequence_number);
    ++next_unacked;
    DiscardPacket(sequence_number);
    return next_unacked;
  }

  SequenceNumberSet* previous_transmissions =
      previous_it->second.previous_transmissions;
  DCHECK(!previous_transmissions->empty());
  SequenceNumberSet::reverse_iterator previous_transmissions_it =
      previous_transmissions->rbegin();
  QuicPacketSequenceNumber newest_transmission = *previous_transmissions_it;
  if (newest_transmission == sequence_number) {
    DiscardPacket(newest_transmission);
  } else {
    // If we have received an ack for a previous transmission of a packet,
    // we want to keep the "new" transmission of the packet unacked,
    // but prevent the data from being retransmitted.
    delete unacked_packets_[newest_transmission].retransmittable_frames;
    unacked_packets_[newest_transmission].retransmittable_frames = NULL;
    unacked_packets_[newest_transmission].previous_transmissions = NULL;
    pending_retransmissions_.erase(newest_transmission);
  }

  // Clear out information all previous transmissions.
  ++previous_transmissions_it;
  while (previous_transmissions_it != previous_transmissions->rend()) {
    QuicPacketSequenceNumber previous_transmission = *previous_transmissions_it;
    ++previous_transmissions_it;
    DiscardPacket(previous_transmission);
  }

  delete previous_transmissions;

  UnackedPacketMap::iterator next_unacked = unacked_packets_.begin();
  while (next_unacked != unacked_packets_.end() &&
         next_unacked->first < sequence_number) {
    ++next_unacked;
  }
  return next_unacked;
}

void QuicSentPacketManager::DiscardPacket(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator unacked_it =
      unacked_packets_.find(sequence_number);
  // Packet was not meant to be retransmitted.
  if (unacked_it == unacked_packets_.end()) {
    return;
  }

  // Delete the retransmittable frames.
  delete unacked_it->second.retransmittable_frames;
  unacked_packets_.erase(unacked_it);
  pending_retransmissions_.erase(sequence_number);
  return;
}

bool QuicSentPacketManager::IsUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return ContainsKey(unacked_packets_, sequence_number);
}

QuicSequenceNumberLength QuicSentPacketManager::GetSequenceNumberLength(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  return unacked_packets_.find(sequence_number)->second.sequence_number_length;
}

bool QuicSentPacketManager::HasUnackedPackets() const {
  return !unacked_packets_.empty();
}

size_t QuicSentPacketManager::GetNumRetransmittablePackets() const {
  size_t num_unacked_packets = 0;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (HasRetransmittableFrames(sequence_number)) {
      ++num_unacked_packets;
    }
  }
  return num_unacked_packets;
}

QuicPacketSequenceNumber
QuicSentPacketManager::GetLeastUnackedSentPacket() const {
  if (unacked_packets_.empty()) {
    // If there are no unacked packets, set the least unacked packet to
    // the sequence number of the next packet sent.
    return helper_->GetNextPacketSequenceNumber();
  }

  return unacked_packets_.begin()->first;
}

SequenceNumberSet QuicSentPacketManager::GetUnackedPackets() const {
  SequenceNumberSet unacked_packets;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    unacked_packets.insert(it->first);
  }
  return unacked_packets;
}

void QuicSentPacketManager::OnPacketSent(
    QuicPacketSequenceNumber sequence_number,
    QuicTime sent_time,
    QuicByteCount bytes,
    TransmissionType transmission_type,
    HasRetransmittableData has_retransmittable_data) {
  DCHECK_LT(0u, sequence_number);
  DCHECK(!ContainsKey(pending_packets_, sequence_number));
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  // Only track packets the send algorithm wants us to track.
  if (!send_algorithm_->OnPacketSent(sent_time, sequence_number, bytes,
                                     transmission_type,
                                     has_retransmittable_data)) {
    DCHECK(unacked_packets_[sequence_number].retransmittable_frames == NULL);
    unacked_packets_.erase(sequence_number);
    return;
  }

  unacked_packets_[sequence_number].sent_time = sent_time;
  packet_history_map_[sequence_number] =
      new SendAlgorithmInterface::SentPacket(bytes, sent_time);
  pending_packets_.insert(sequence_number);
  CleanupPacketHistory();
}

void QuicSentPacketManager::OnRetransmissionTimeout() {
  // Abandon all pending packets to ensure the congestion window
  // opens up before we attempt to retransmit packets.
  QuicTime::Delta retransmission_delay = GetRetransmissionDelay();
  QuicTime max_send_time =
      clock_->ApproximateNow().Subtract(retransmission_delay);

  // Attempt to send all the unacked packets when the RTO fires, let the
  // congestion manager decide how many to send immediately and the remaining
  // packets will be queued for future sending.
  DVLOG(1) << "OnRetransmissionTimeout() fired with "
           << unacked_packets_.size() << " unacked packets.";

  // Retransmit any packet with retransmittable frames and abandon the rest.
  bool packets_retransmitted = false;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    if (it->second.retransmittable_frames != NULL) {
      OnPacketAbandoned(it->first);
      packets_retransmitted = true;
      MarkForRetransmission(it->first, RTO_RETRANSMISSION);
    } else if (it->second.sent_time <= max_send_time) {
      OnPacketAbandoned(it->first);
    }
  }

  // Only inform the sent packet manager of an RTO if data was retransmitted.
  if (packets_retransmitted) {
    ++consecutive_rto_count_;
    send_algorithm_->OnRetransmissionTimeout();
  }
}

void QuicSentPacketManager::OnPacketAbandoned(
    QuicPacketSequenceNumber sequence_number) {
  SequenceNumberSet::iterator it = pending_packets_.find(sequence_number);
  if (it != pending_packets_.end()) {
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    send_algorithm_->OnPacketAbandoned(
        sequence_number, packet_history_map_[sequence_number]->bytes_sent());
    pending_packets_.erase(it);
  }
}

void QuicSentPacketManager::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame,
    const QuicTime& feedback_receive_time) {
  send_algorithm_->OnIncomingQuicCongestionFeedbackFrame(
      frame, feedback_receive_time, packet_history_map_);
}

void QuicSentPacketManager::MaybeRetransmitOnAckFrame(
    const ReceivedPacketInfo& received_info,
    const QuicTime& ack_receive_time) {
  // Go through all pending packets up to the largest observed and see if any
  // need to be retransmitted or lost.
  SequenceNumberSet::iterator it = pending_packets_.begin();
  SequenceNumberSet::iterator it_upper =
      pending_packets_.upper_bound(received_info.largest_observed);

  size_t num_retransmitted = 0;
  SequenceNumberSet lost_packets;
  while (it != it_upper) {
    QuicPacketSequenceNumber sequence_number = *it;
    DVLOG(1) << "still missing packet " << sequence_number;
    // Acks must be handled previously, so ensure it's missing and not acked.
    DCHECK(IsAwaitingPacket(received_info, sequence_number));
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    const SendAlgorithmInterface::SentPacket* sent_packet =
        packet_history_map_[sequence_number];
    const TransmissionInfo& transmission_info =
        unacked_packets_[sequence_number];

    // Consider it multiple nacks when there is a gap between the missing packet
    // and the largest observed, since the purpose of a nack threshold is to
    // tolerate re-ordering.  This handles both StretchAcks and Forward Acks.
    // TODO(ianswett): This relies heavily on sequential reception of packets,
    // and makes an assumption that the congestion control uses TCP style nacks.
    size_t min_nacks = received_info.largest_observed - sequence_number;
    packet_history_map_[sequence_number]->Nack(min_nacks);

    size_t num_nacks_needed = kNumberOfNacksBeforeRetransmission;
    // Check for early retransmit(RFC5827) when the last packet gets acked and
    // the there are fewer than 4 pending packets.
    if (pending_packets_.size() <= kNumberOfNacksBeforeRetransmission &&
        transmission_info.retransmittable_frames &&
        packet_history_map_.rbegin()->first == received_info.largest_observed) {
      num_nacks_needed = received_info.largest_observed - sequence_number;
    }

    if (sent_packet->nack_count() < num_nacks_needed) {
      ++it;
      continue;
    }

    // If the number of retransmissions has maxed out, don't lose or retransmit
    // any more packets.
    if (num_retransmitted >= kMaxRetransmissionsPerAck) {
      ++it;
      continue;
    }

    lost_packets.insert(sequence_number);
    if (transmission_info.retransmittable_frames) {
      ++num_retransmitted;
      MarkForRetransmission(*it, NACK_RETRANSMISSION);
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
}

void QuicSentPacketManager::MaybeUpdateRTT(
    const ReceivedPacketInfo& received_info,
    const QuicTime& ack_receive_time) {
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  SendAlgorithmInterface::SentPacketsMap::iterator history_it =
      packet_history_map_.find(received_info.largest_observed);
  // TODO(satyamshekhar): largest_observed might be missing.
  if (history_it == packet_history_map_.end()) {
    return;
  }

  QuicTime::Delta send_delta = ack_receive_time.Subtract(
      history_it->second->send_timestamp());
  if (send_delta > received_info.delta_time_largest_observed) {
    rtt_sample_ = send_delta.Subtract(
        received_info.delta_time_largest_observed);
  } else if (rtt_sample_.IsInfinite()) {
    // Even though we received information from the peer suggesting
    // an invalid (negative) RTT, we can use the send delta as an
    // approximation until we get a better estimate.
    rtt_sample_ = send_delta;
  }
}

QuicTime::Delta QuicSentPacketManager::TimeUntilSend(
    QuicTime now,
    TransmissionType transmission_type,
    HasRetransmittableData retransmittable,
    IsHandshake handshake) {
  return send_algorithm_->TimeUntilSend(now, transmission_type, retransmittable,
                                        handshake);
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

const QuicTime::Delta QuicSentPacketManager::DelayedAckTime() {
  return QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs/2);
}

const QuicTime QuicSentPacketManager::GetRetransmissionTime() const {
  if (pending_packets_.empty()) {
    return QuicTime::Zero();
  }
  return clock_->ApproximateNow().Add(GetRetransmissionDelay());
}

const QuicTime::Delta QuicSentPacketManager::GetRetransmissionDelay() const {
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

const QuicTime::Delta QuicSentPacketManager::SmoothedRtt() const {
  return send_algorithm_->SmoothedRtt();
}

QuicBandwidth QuicSentPacketManager::BandwidthEstimate() const {
  return send_algorithm_->BandwidthEstimate();
}

QuicByteCount QuicSentPacketManager::GetCongestionWindow() const {
  return send_algorithm_->GetCongestionWindow();
}

void QuicSentPacketManager::CleanupPacketHistory() {
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

void QuicSentPacketManager::MaybeEnablePacing() {
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
