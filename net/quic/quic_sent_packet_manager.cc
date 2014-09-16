// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/congestion_control/pacing_sender.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_ack_notifier_manager.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils_chromium.h"

using std::make_pair;
using std::max;
using std::min;

namespace net {

// The length of the recent min rtt window in seconds. Windowing is disabled for
// values less than or equal to 0.
int32 FLAGS_quic_recent_min_rtt_window_s = 60;

namespace {
static const int kDefaultRetransmissionTimeMs = 500;
// TCP RFC calls for 1 second RTO however Linux differs from this default and
// define the minimum RTO to 200ms, we will use the same until we have data to
// support a higher or lower value.
static const int kMinRetransmissionTimeMs = 200;
static const int kMaxRetransmissionTimeMs = 60000;
static const size_t kMaxRetransmissions = 10;

// Only exponentially back off the handshake timer 5 times due to a timeout.
static const size_t kMaxHandshakeRetransmissionBackoffs = 5;
static const size_t kMinHandshakeTimeoutMs = 10;

// Sends up to two tail loss probes before firing an RTO,
// per draft RFC draft-dukkipati-tcpm-tcp-loss-probe.
static const size_t kDefaultMaxTailLossProbes = 2;
static const int64 kMinTailLossProbeTimeoutMs = 10;

// Number of samples before we force a new recent min rtt to be captured.
static const size_t kNumMinRttSamplesAfterQuiescence = 2;

// Number of unpaced packets to send after quiescence.
static const size_t kInitialUnpacedBurst = 10;

bool HasCryptoHandshake(const TransmissionInfo& transmission_info) {
  if (transmission_info.retransmittable_frames == NULL) {
    return false;
  }
  return transmission_info.retransmittable_frames->HasCryptoHandshake() ==
      IS_HANDSHAKE;
}

}  // namespace

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicSentPacketManager::QuicSentPacketManager(
    bool is_server,
    const QuicClock* clock,
    QuicConnectionStats* stats,
    CongestionControlType congestion_control_type,
    LossDetectionType loss_type)
    : unacked_packets_(),
      is_server_(is_server),
      clock_(clock),
      stats_(stats),
      debug_delegate_(NULL),
      network_change_visitor_(NULL),
      send_algorithm_(SendAlgorithmInterface::Create(clock,
                                                     &rtt_stats_,
                                                     congestion_control_type,
                                                     stats)),
      loss_algorithm_(LossDetectionInterface::Create(loss_type)),
      least_packet_awaited_by_peer_(1),
      first_rto_transmission_(0),
      consecutive_rto_count_(0),
      consecutive_tlp_count_(0),
      consecutive_crypto_retransmission_count_(0),
      pending_timer_transmission_count_(0),
      max_tail_loss_probes_(kDefaultMaxTailLossProbes),
      using_pacing_(false),
      handshake_confirmed_(false) {
}

QuicSentPacketManager::~QuicSentPacketManager() {
}

void QuicSentPacketManager::SetFromConfig(const QuicConfig& config) {
  if (config.HasReceivedInitialRoundTripTimeUs() &&
      config.ReceivedInitialRoundTripTimeUs() > 0) {
    rtt_stats_.set_initial_rtt_us(min(kMaxInitialRoundTripTimeUs,
                                      config.ReceivedInitialRoundTripTimeUs()));
  } else if (config.HasInitialRoundTripTimeUsToSend()) {
    rtt_stats_.set_initial_rtt_us(
        min(kMaxInitialRoundTripTimeUs,
            config.GetInitialRoundTripTimeUsToSend()));
  }
  // TODO(ianswett): BBR is currently a server only feature.
  if (config.HasReceivedConnectionOptions() &&
      ContainsQuicTag(config.ReceivedConnectionOptions(), kTBBR)) {
    if (FLAGS_quic_recent_min_rtt_window_s > 0) {
      rtt_stats_.set_recent_min_rtt_window(
          QuicTime::Delta::FromSeconds(FLAGS_quic_recent_min_rtt_window_s));
    }
    send_algorithm_.reset(
        SendAlgorithmInterface::Create(clock_, &rtt_stats_, kBBR, stats_));
  }
  if (config.HasReceivedConnectionOptions() &&
      ContainsQuicTag(config.ReceivedConnectionOptions(), kRENO)) {
    send_algorithm_.reset(
        SendAlgorithmInterface::Create(clock_, &rtt_stats_, kReno, stats_));
  }
  if (is_server_) {
    if (config.HasReceivedConnectionOptions() &&
        ContainsQuicTag(config.ReceivedConnectionOptions(), kPACE)) {
      EnablePacing();
    }
  } else if (config.HasSendConnectionOptions() &&
             ContainsQuicTag(config.SendConnectionOptions(), kPACE)) {
    EnablePacing();
  }
  // TODO(ianswett): Remove the "HasReceivedLossDetection" branch once
  // the ConnectionOptions code is live everywhere.
  if ((config.HasReceivedLossDetection() &&
       config.ReceivedLossDetection() == kTIME) ||
      (config.HasReceivedConnectionOptions() &&
       ContainsQuicTag(config.ReceivedConnectionOptions(), kTIME))) {
    loss_algorithm_.reset(LossDetectionInterface::Create(kTime));
  }
  send_algorithm_->SetFromConfig(config, is_server_);

  if (network_change_visitor_ != NULL) {
    network_change_visitor_->OnCongestionWindowChange(GetCongestionWindow());
  }
}

// TODO(ianswett): Combine this method with OnPacketSent once packets are always
// sent in order and the connection tracks RetransmittableFrames for longer.
void QuicSentPacketManager::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  if (serialized_packet.retransmittable_frames) {
    ack_notifier_manager_.OnSerializedPacket(serialized_packet);
  }
  unacked_packets_.AddPacket(serialized_packet);

  if (debug_delegate_ != NULL) {
    debug_delegate_->OnSerializedPacket(serialized_packet);
  }
}

void QuicSentPacketManager::OnRetransmittedPacket(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number) {
  TransmissionType transmission_type;
  PendingRetransmissionMap::iterator it =
      pending_retransmissions_.find(old_sequence_number);
  if (it != pending_retransmissions_.end()) {
    transmission_type = it->second;
    pending_retransmissions_.erase(it);
  } else {
    DLOG(DFATAL) << "Expected sequence number to be in "
        "pending_retransmissions_.  sequence_number: " << old_sequence_number;
    transmission_type = NOT_RETRANSMISSION;
  }

  // A notifier may be waiting to hear about ACKs for the original sequence
  // number. Inform them that the sequence number has changed.
  ack_notifier_manager_.UpdateSequenceNumber(old_sequence_number,
                                             new_sequence_number);

  unacked_packets_.OnRetransmittedPacket(old_sequence_number,
                                         new_sequence_number,
                                         transmission_type);

  if (debug_delegate_ != NULL) {
    debug_delegate_->OnRetransmittedPacket(old_sequence_number,
        new_sequence_number,
        transmission_type,
        clock_->ApproximateNow());
  }
}

void QuicSentPacketManager::OnIncomingAck(const QuicAckFrame& ack_frame,
                                          QuicTime ack_receive_time) {
  QuicByteCount bytes_in_flight = unacked_packets_.bytes_in_flight();

  UpdatePacketInformationReceivedByPeer(ack_frame);
  // We rely on delta_time_largest_observed to compute an RTT estimate, so
  // we only update rtt when the largest observed gets acked.
  bool largest_observed_acked = MaybeUpdateRTT(ack_frame, ack_receive_time);
  DCHECK_GE(ack_frame.largest_observed, unacked_packets_.largest_observed());
  unacked_packets_.IncreaseLargestObserved(ack_frame.largest_observed);

  HandleAckForSentPackets(ack_frame);
  InvokeLossDetection(ack_receive_time);
  MaybeInvokeCongestionEvent(largest_observed_acked, bytes_in_flight);
  unacked_packets_.RemoveObsoletePackets();

  sustained_bandwidth_recorder_.RecordEstimate(
      send_algorithm_->InRecovery(),
      send_algorithm_->InSlowStart(),
      send_algorithm_->BandwidthEstimate(),
      ack_receive_time,
      clock_->WallNow(),
      rtt_stats_.SmoothedRtt());

  // If we have received a truncated ack, then we need to clear out some
  // previous transmissions to allow the peer to actually ACK new packets.
  if (ack_frame.is_truncated) {
    unacked_packets_.ClearAllPreviousRetransmissions();
  }

  // Anytime we are making forward progress and have a new RTT estimate, reset
  // the backoff counters.
  if (largest_observed_acked) {
    // Reset all retransmit counters any time a new packet is acked.
    consecutive_rto_count_ = 0;
    consecutive_tlp_count_ = 0;
    consecutive_crypto_retransmission_count_ = 0;
  }

  if (debug_delegate_ != NULL) {
    debug_delegate_->OnIncomingAck(ack_frame,
                                   ack_receive_time,
                                   unacked_packets_.largest_observed(),
                                   largest_observed_acked,
                                   GetLeastUnacked());
  }
}

void QuicSentPacketManager::UpdatePacketInformationReceivedByPeer(
    const QuicAckFrame& ack_frame) {
  if (ack_frame.missing_packets.empty()) {
    least_packet_awaited_by_peer_ = ack_frame.largest_observed + 1;
  } else {
    least_packet_awaited_by_peer_ = *(ack_frame.missing_packets.begin());
  }
}

void QuicSentPacketManager::MaybeInvokeCongestionEvent(
    bool rtt_updated, QuicByteCount bytes_in_flight) {
  if (!rtt_updated && packets_acked_.empty() && packets_lost_.empty()) {
    return;
  }
  send_algorithm_->OnCongestionEvent(rtt_updated, bytes_in_flight,
                                     packets_acked_, packets_lost_);
  packets_acked_.clear();
  packets_lost_.clear();
  if (network_change_visitor_ != NULL) {
    network_change_visitor_->OnCongestionWindowChange(GetCongestionWindow());
  }
}

void QuicSentPacketManager::HandleAckForSentPackets(
    const QuicAckFrame& ack_frame) {
  // Go through the packets we have not received an ack for and see if this
  // incoming_ack shows they've been seen by the peer.
  QuicTime::Delta delta_largest_observed =
      ack_frame.delta_time_largest_observed;
  QuicPacketSequenceNumber sequence_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++sequence_number) {
    if (sequence_number > ack_frame.largest_observed) {
      // These packets are still in flight.
      break;
    }

    if (ContainsKey(ack_frame.missing_packets, sequence_number)) {
      // Don't continue to increase the nack count for packets not in flight.
      if (!it->in_flight) {
        continue;
      }
      // Consider it multiple nacks when there is a gap between the missing
      // packet and the largest observed, since the purpose of a nack
      // threshold is to tolerate re-ordering.  This handles both StretchAcks
      // and Forward Acks.
      // The nack count only increases when the largest observed increases.
      size_t min_nacks = ack_frame.largest_observed - sequence_number;
      // Truncated acks can nack the largest observed, so use a min of 1.
      if (min_nacks == 0) {
        min_nacks = 1;
      }
      unacked_packets_.NackPacket(sequence_number, min_nacks);
      continue;
    }
    // Packet was acked, so remove it from our unacked packet list.
    DVLOG(1) << ENDPOINT << "Got an ack for packet " << sequence_number;
    // If data is associated with the most recent transmission of this
    // packet, then inform the caller.
    if (it->in_flight) {
      packets_acked_.push_back(make_pair(sequence_number, *it));
    }
    MarkPacketHandled(sequence_number, *it, delta_largest_observed);
  }

  // Discard any retransmittable frames associated with revived packets.
  for (SequenceNumberSet::const_iterator revived_it =
           ack_frame.revived_packets.begin();
       revived_it != ack_frame.revived_packets.end(); ++revived_it) {
    MarkPacketRevived(*revived_it, delta_largest_observed);
  }
}

bool QuicSentPacketManager::HasRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  return unacked_packets_.HasRetransmittableFrames(sequence_number);
}

void QuicSentPacketManager::RetransmitUnackedPackets(
    TransmissionType retransmission_type) {
  DCHECK(retransmission_type == ALL_UNACKED_RETRANSMISSION ||
         retransmission_type == ALL_INITIAL_RETRANSMISSION);
  QuicPacketSequenceNumber sequence_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++sequence_number) {
    const RetransmittableFrames* frames = it->retransmittable_frames;
    if (frames != NULL && (retransmission_type == ALL_UNACKED_RETRANSMISSION ||
                           frames->encryption_level() == ENCRYPTION_INITIAL)) {
      MarkForRetransmission(sequence_number, retransmission_type);
    }
  }
}

void QuicSentPacketManager::NeuterUnencryptedPackets() {
  QuicPacketSequenceNumber sequence_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++sequence_number) {
    const RetransmittableFrames* frames = it->retransmittable_frames;
    if (frames != NULL && frames->encryption_level() == ENCRYPTION_NONE) {
      // Once you're forward secure, no unencrypted packets will be sent, crypto
      // or otherwise. Unencrypted packets are neutered and abandoned, to ensure
      // they are not retransmitted or considered lost from a congestion control
      // perspective.
      pending_retransmissions_.erase(sequence_number);
      unacked_packets_.RemoveFromInFlight(sequence_number);
      unacked_packets_.RemoveRetransmittability(sequence_number);
    }
  }
}

void QuicSentPacketManager::MarkForRetransmission(
    QuicPacketSequenceNumber sequence_number,
    TransmissionType transmission_type) {
  const TransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(sequence_number);
  LOG_IF(DFATAL, transmission_info.retransmittable_frames == NULL);
  if (transmission_type != TLP_RETRANSMISSION) {
    unacked_packets_.RemoveFromInFlight(sequence_number);
  }
  // TODO(ianswett): Currently the RTO can fire while there are pending NACK
  // retransmissions for the same data, which is not ideal.
  if (ContainsKey(pending_retransmissions_, sequence_number)) {
    return;
  }

  pending_retransmissions_[sequence_number] = transmission_type;
}

void QuicSentPacketManager::RecordSpuriousRetransmissions(
    const SequenceNumberList& all_transmissions,
    QuicPacketSequenceNumber acked_sequence_number) {
  if (acked_sequence_number < first_rto_transmission_) {
    // Cancel all pending RTO transmissions and restore their in flight status.
    // Replace SRTT with latest_rtt and increase the variance to prevent
    // a spurious RTO from happening again.
    rtt_stats_.ExpireSmoothedMetrics();
    for (PendingRetransmissionMap::const_iterator it =
             pending_retransmissions_.begin();
         it != pending_retransmissions_.end(); ++it) {
      DCHECK_EQ(it->second, RTO_RETRANSMISSION);
      unacked_packets_.RestoreInFlight(it->first);
    }
    pending_retransmissions_.clear();
    send_algorithm_->RevertRetransmissionTimeout();
    first_rto_transmission_ = 0;
    ++stats_->spurious_rto_count;
  }
  for (SequenceNumberList::const_reverse_iterator it =
           all_transmissions.rbegin();
       it != all_transmissions.rend() && *it > acked_sequence_number; ++it) {
    const TransmissionInfo& retransmit_info =
        unacked_packets_.GetTransmissionInfo(*it);

    stats_->bytes_spuriously_retransmitted += retransmit_info.bytes_sent;
    ++stats_->packets_spuriously_retransmitted;
    if (debug_delegate_ != NULL) {
      debug_delegate_->OnSpuriousPacketRetransmition(
          retransmit_info.transmission_type,
          retransmit_info.bytes_sent);
    }
  }
}

bool QuicSentPacketManager::HasPendingRetransmissions() const {
  return !pending_retransmissions_.empty();
}

QuicSentPacketManager::PendingRetransmission
    QuicSentPacketManager::NextPendingRetransmission() {
  DCHECK(!pending_retransmissions_.empty());
  QuicPacketSequenceNumber sequence_number =
      pending_retransmissions_.begin()->first;
  TransmissionType transmission_type = pending_retransmissions_.begin()->second;
  if (unacked_packets_.HasPendingCryptoPackets()) {
    // Ensure crypto packets are retransmitted before other packets.
    PendingRetransmissionMap::const_iterator it =
        pending_retransmissions_.begin();
    do {
      if (HasCryptoHandshake(unacked_packets_.GetTransmissionInfo(it->first))) {
        sequence_number = it->first;
        transmission_type = it->second;
        break;
      }
      ++it;
    } while (it != pending_retransmissions_.end());
  }
  DCHECK(unacked_packets_.IsUnacked(sequence_number)) << sequence_number;
  const TransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(sequence_number);
  DCHECK(transmission_info.retransmittable_frames);

  return PendingRetransmission(sequence_number,
                               transmission_type,
                               *transmission_info.retransmittable_frames,
                               transmission_info.sequence_number_length);
}

void QuicSentPacketManager::MarkPacketRevived(
    QuicPacketSequenceNumber sequence_number,
    QuicTime::Delta delta_largest_observed) {
  if (!unacked_packets_.IsUnacked(sequence_number)) {
    return;
  }

  const TransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(sequence_number);
  QuicPacketSequenceNumber newest_transmission =
      transmission_info.all_transmissions == NULL ?
          sequence_number : *transmission_info.all_transmissions->rbegin();
  // This packet has been revived at the receiver. If we were going to
  // retransmit it, do not retransmit it anymore.
  pending_retransmissions_.erase(newest_transmission);

  // The AckNotifierManager needs to be notified for revived packets,
  // since it indicates the packet arrived from the appliction's perspective.
  if (transmission_info.retransmittable_frames) {
    ack_notifier_manager_.OnPacketAcked(
        newest_transmission, delta_largest_observed);
  }

  unacked_packets_.RemoveRetransmittability(sequence_number);
}

void QuicSentPacketManager::MarkPacketHandled(
    QuicPacketSequenceNumber sequence_number,
    const TransmissionInfo& info,
    QuicTime::Delta delta_largest_observed) {
  QuicPacketSequenceNumber newest_transmission =
      info.all_transmissions == NULL ?
          sequence_number : *info.all_transmissions->rbegin();
  // Remove the most recent packet, if it is pending retransmission.
  pending_retransmissions_.erase(newest_transmission);

  // The AckNotifierManager needs to be notified about the most recent
  // transmission, since that's the one only one it tracks.
  ack_notifier_manager_.OnPacketAcked(newest_transmission,
                                      delta_largest_observed);
  if (newest_transmission != sequence_number) {
    RecordSpuriousRetransmissions(*info.all_transmissions, sequence_number);
    // Remove the most recent packet from flight if it's a crypto handshake
    // packet, since they won't be acked now that one has been processed.
    // Other crypto handshake packets won't be in flight, only the newest
    // transmission of a crypto packet is in flight at once.
    // TODO(ianswett): Instead of handling all crypto packets special,
    // only handle NULL encrypted packets in a special way.
    if (HasCryptoHandshake(
        unacked_packets_.GetTransmissionInfo(newest_transmission))) {
      unacked_packets_.RemoveFromInFlight(newest_transmission);
    }
  }

  unacked_packets_.RemoveFromInFlight(sequence_number);
  unacked_packets_.RemoveRetransmittability(sequence_number);
}

bool QuicSentPacketManager::IsUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return unacked_packets_.IsUnacked(sequence_number);
}

bool QuicSentPacketManager::HasUnackedPackets() const {
  return unacked_packets_.HasUnackedPackets();
}

QuicPacketSequenceNumber
QuicSentPacketManager::GetLeastUnacked() const {
  return unacked_packets_.GetLeastUnacked();
}

bool QuicSentPacketManager::OnPacketSent(
    QuicPacketSequenceNumber sequence_number,
    QuicTime sent_time,
    QuicByteCount bytes,
    TransmissionType transmission_type,
    HasRetransmittableData has_retransmittable_data) {
  DCHECK_LT(0u, sequence_number);
  DCHECK(unacked_packets_.IsUnacked(sequence_number));
  LOG_IF(DFATAL, bytes == 0) << "Cannot send empty packets.";
  if (pending_timer_transmission_count_ > 0) {
    --pending_timer_transmission_count_;
  }

  if (unacked_packets_.bytes_in_flight() == 0) {
    // TODO(ianswett): Consider being less aggressive to force a new
    // recent_min_rtt, likely by not discarding a relatively new sample.
    DVLOG(1) << "Sampling a new recent min rtt within 2 samples. currently:"
             << rtt_stats_.recent_min_rtt().ToMilliseconds() << "ms";
    rtt_stats_.SampleNewRecentMinRtt(kNumMinRttSamplesAfterQuiescence);
  }

  // Only track packets as in flight that the send algorithm wants us to track.
  const bool in_flight =
      send_algorithm_->OnPacketSent(sent_time,
                                    unacked_packets_.bytes_in_flight(),
                                    sequence_number,
                                    bytes,
                                    has_retransmittable_data);
  unacked_packets_.SetSent(sequence_number, sent_time, bytes, in_flight);

  if (debug_delegate_ != NULL) {
    debug_delegate_->OnSentPacket(
        sequence_number, sent_time, bytes, transmission_type);
  }

  // Reset the retransmission timer anytime a pending packet is sent.
  return in_flight;
}

void QuicSentPacketManager::OnRetransmissionTimeout() {
  DCHECK(unacked_packets_.HasInFlightPackets());
  DCHECK_EQ(0u, pending_timer_transmission_count_);
  // Handshake retransmission, timer based loss detection, TLP, and RTO are
  // implemented with a single alarm. The handshake alarm is set when the
  // handshake has not completed, the loss alarm is set when the loss detection
  // algorithm says to, and the TLP and  RTO alarms are set after that.
  // The TLP alarm is always set to run for under an RTO.
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      ++stats_->crypto_retransmit_count;
      RetransmitCryptoPackets();
      return;
    case LOSS_MODE: {
      ++stats_->loss_timeout_count;
      QuicByteCount bytes_in_flight = unacked_packets_.bytes_in_flight();
      InvokeLossDetection(clock_->Now());
      MaybeInvokeCongestionEvent(false, bytes_in_flight);
      return;
    }
    case TLP_MODE:
      // If no tail loss probe can be sent, because there are no retransmittable
      // packets, execute a conventional RTO to abandon old packets.
      ++stats_->tlp_count;
      ++consecutive_tlp_count_;
      pending_timer_transmission_count_ = 1;
      // TLPs prefer sending new data instead of retransmitting data, so
      // give the connection a chance to write before completing the TLP.
      return;
    case RTO_MODE:
      ++stats_->rto_count;
      RetransmitAllPackets();
      return;
  }
}

void QuicSentPacketManager::RetransmitCryptoPackets() {
  DCHECK_EQ(HANDSHAKE_MODE, GetRetransmissionMode());
  // TODO(ianswett): Typical TCP implementations only retransmit 5 times.
  consecutive_crypto_retransmission_count_ =
      min(kMaxHandshakeRetransmissionBackoffs,
          consecutive_crypto_retransmission_count_ + 1);
  bool packet_retransmitted = false;
  QuicPacketSequenceNumber sequence_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++sequence_number) {
    // Only retransmit frames which are in flight, and therefore have been sent.
    if (!it->in_flight || it->retransmittable_frames == NULL ||
        it->retransmittable_frames->HasCryptoHandshake() != IS_HANDSHAKE) {
      continue;
    }
    packet_retransmitted = true;
    MarkForRetransmission(sequence_number, HANDSHAKE_RETRANSMISSION);
    ++pending_timer_transmission_count_;
  }
  DCHECK(packet_retransmitted) << "No crypto packets found to retransmit.";
}

bool QuicSentPacketManager::MaybeRetransmitTailLossProbe() {
  if (pending_timer_transmission_count_ == 0) {
    return false;
  }
  QuicPacketSequenceNumber sequence_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++sequence_number) {
    // Only retransmit frames which are in flight, and therefore have been sent.
    if (!it->in_flight || it->retransmittable_frames == NULL) {
      continue;
    }
    if (!handshake_confirmed_) {
      DCHECK_NE(IS_HANDSHAKE, it->retransmittable_frames->HasCryptoHandshake());
    }
    MarkForRetransmission(sequence_number, TLP_RETRANSMISSION);
    return true;
  }
  DLOG(FATAL)
    << "No retransmittable packets, so RetransmitOldestPacket failed.";
  return false;
}

void QuicSentPacketManager::RetransmitAllPackets() {
  DVLOG(1) << "RetransmitAllPackets() called with "
           << unacked_packets_.GetNumUnackedPacketsDebugOnly()
           << " unacked packets.";
  // Request retransmission of all retransmittable packets when the RTO
  // fires, and let the congestion manager decide how many to send
  // immediately and the remaining packets will be queued.
  // Abandon any non-retransmittable packets that are sufficiently old.
  bool packets_retransmitted = false;
  QuicPacketSequenceNumber sequence_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++sequence_number) {
    if (it->retransmittable_frames != NULL) {
      packets_retransmitted = true;
      MarkForRetransmission(sequence_number, RTO_RETRANSMISSION);
    } else {
      unacked_packets_.RemoveFromInFlight(sequence_number);
    }
  }

  send_algorithm_->OnRetransmissionTimeout(packets_retransmitted);
  if (packets_retransmitted) {
    if (consecutive_rto_count_ == 0) {
      first_rto_transmission_ = unacked_packets_.largest_sent_packet() + 1;
    }
    ++consecutive_rto_count_;
  }

  if (network_change_visitor_ != NULL) {
    network_change_visitor_->OnCongestionWindowChange(GetCongestionWindow());
  }
}

QuicSentPacketManager::RetransmissionTimeoutMode
    QuicSentPacketManager::GetRetransmissionMode() const {
  DCHECK(unacked_packets_.HasInFlightPackets());
  if (!handshake_confirmed_ && unacked_packets_.HasPendingCryptoPackets()) {
    return HANDSHAKE_MODE;
  }
  if (loss_algorithm_->GetLossTimeout() != QuicTime::Zero()) {
    return LOSS_MODE;
  }
  if (consecutive_tlp_count_ < max_tail_loss_probes_) {
    if (unacked_packets_.HasUnackedRetransmittableFrames()) {
      return TLP_MODE;
    }
  }
  return RTO_MODE;
}

void QuicSentPacketManager::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame,
    const QuicTime& feedback_receive_time) {
  send_algorithm_->OnIncomingQuicCongestionFeedbackFrame(
      frame, feedback_receive_time);
}

void QuicSentPacketManager::InvokeLossDetection(QuicTime time) {
  SequenceNumberSet lost_packets =
      loss_algorithm_->DetectLostPackets(unacked_packets_,
                                         time,
                                         unacked_packets_.largest_observed(),
                                         rtt_stats_);
  for (SequenceNumberSet::const_iterator it = lost_packets.begin();
       it != lost_packets.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = *it;
    const TransmissionInfo& transmission_info =
        unacked_packets_.GetTransmissionInfo(sequence_number);
    // TODO(ianswett): If it's expected the FEC packet may repair the loss, it
    // should be recorded as a loss to the send algorithm, but not retransmitted
    // until it's known whether the FEC packet arrived.
    ++stats_->packets_lost;
    packets_lost_.push_back(make_pair(sequence_number, transmission_info));
    DVLOG(1) << ENDPOINT << "Lost packet " << sequence_number;

    if (transmission_info.retransmittable_frames != NULL) {
      MarkForRetransmission(sequence_number, LOSS_RETRANSMISSION);
    } else {
      // Since we will not retransmit this, we need to remove it from
      // unacked_packets_.   This is either the current transmission of
      // a packet whose previous transmission has been acked, a packet that has
      // been TLP retransmitted, or an FEC packet.
      unacked_packets_.RemoveFromInFlight(sequence_number);
    }
  }
}

bool QuicSentPacketManager::MaybeUpdateRTT(
    const QuicAckFrame& ack_frame,
    const QuicTime& ack_receive_time) {
  if (!unacked_packets_.IsUnacked(ack_frame.largest_observed)) {
    return false;
  }
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  const TransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(ack_frame.largest_observed);
  // Ensure the packet has a valid sent time.
  if (transmission_info.sent_time == QuicTime::Zero()) {
    LOG(DFATAL) << "Acked packet has zero sent time, largest_observed:"
                << ack_frame.largest_observed;
    return false;
  }

  QuicTime::Delta send_delta =
      ack_receive_time.Subtract(transmission_info.sent_time);
  rtt_stats_.UpdateRtt(
      send_delta, ack_frame.delta_time_largest_observed, ack_receive_time);
  return true;
}

QuicTime::Delta QuicSentPacketManager::TimeUntilSend(
    QuicTime now,
    HasRetransmittableData retransmittable) {
  // The TLP logic is entirely contained within QuicSentPacketManager, so the
  // send algorithm does not need to be consulted.
  if (pending_timer_transmission_count_ > 0) {
    return QuicTime::Delta::Zero();
  }
  return send_algorithm_->TimeUntilSend(
      now, unacked_packets_.bytes_in_flight(), retransmittable);
}

// Uses a 25ms delayed ack timer. Also helps with better signaling
// in low-bandwidth (< ~384 kbps), where an ack is sent per packet.
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
const QuicTime::Delta QuicSentPacketManager::DelayedAckTime() const {
  return QuicTime::Delta::FromMilliseconds(min(kMaxDelayedAckTimeMs,
                                               kMinRetransmissionTimeMs/2));
}

const QuicTime QuicSentPacketManager::GetRetransmissionTime() const {
  // Don't set the timer if there are no packets in flight or we've already
  // queued a tlp transmission and it hasn't been sent yet.
  if (!unacked_packets_.HasInFlightPackets() ||
      pending_timer_transmission_count_ > 0) {
    return QuicTime::Zero();
  }
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      return clock_->ApproximateNow().Add(GetCryptoRetransmissionDelay());
    case LOSS_MODE:
      return loss_algorithm_->GetLossTimeout();
    case TLP_MODE: {
      // TODO(ianswett): When CWND is available, it would be preferable to
      // set the timer based on the earliest retransmittable packet.
      // Base the updated timer on the send time of the last packet.
      const QuicTime sent_time = unacked_packets_.GetLastPacketSentTime();
      const QuicTime tlp_time = sent_time.Add(GetTailLossProbeDelay());
      // Ensure the TLP timer never gets set to a time in the past.
      return QuicTime::Max(clock_->ApproximateNow(), tlp_time);
    }
    case RTO_MODE: {
      // The RTO is based on the first outstanding packet.
      const QuicTime sent_time =
          unacked_packets_.GetFirstInFlightPacketSentTime();
      QuicTime rto_time = sent_time.Add(GetRetransmissionDelay());
      // Wait for TLP packets to be acked before an RTO fires.
      QuicTime tlp_time =
          unacked_packets_.GetLastPacketSentTime().Add(GetTailLossProbeDelay());
      return QuicTime::Max(tlp_time, rto_time);
    }
  }
  DCHECK(false);
  return QuicTime::Zero();
}

const QuicTime::Delta QuicSentPacketManager::GetCryptoRetransmissionDelay()
    const {
  // This is equivalent to the TailLossProbeDelay, but slightly more aggressive
  // because crypto handshake messages don't incur a delayed ack time.
  int64 delay_ms = max<int64>(kMinHandshakeTimeoutMs,
                              1.5 * rtt_stats_.SmoothedRtt().ToMilliseconds());
  return QuicTime::Delta::FromMilliseconds(
      delay_ms << consecutive_crypto_retransmission_count_);
}

const QuicTime::Delta QuicSentPacketManager::GetTailLossProbeDelay() const {
  QuicTime::Delta srtt = rtt_stats_.SmoothedRtt();
  if (!unacked_packets_.HasMultipleInFlightPackets()) {
    return QuicTime::Delta::Max(
        srtt.Multiply(2), srtt.Multiply(1.5)
          .Add(QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs/2)));
  }
  return QuicTime::Delta::FromMilliseconds(
      max(kMinTailLossProbeTimeoutMs,
          static_cast<int64>(2 * srtt.ToMilliseconds())));
}

const QuicTime::Delta QuicSentPacketManager::GetRetransmissionDelay() const {
  QuicTime::Delta retransmission_delay = send_algorithm_->RetransmissionDelay();
  // TODO(rch): This code should move to |send_algorithm_|.
  if (retransmission_delay.IsZero()) {
    // We are in the initial state, use default timeout values.
    retransmission_delay =
        QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  } else if (retransmission_delay.ToMilliseconds() < kMinRetransmissionTimeMs) {
    retransmission_delay =
        QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs);
  }

  // Calculate exponential back off.
  retransmission_delay = retransmission_delay.Multiply(
      1 << min<size_t>(consecutive_rto_count_, kMaxRetransmissions));

  if (retransmission_delay.ToMilliseconds() > kMaxRetransmissionTimeMs) {
    return QuicTime::Delta::FromMilliseconds(kMaxRetransmissionTimeMs);
  }
  return retransmission_delay;
}

const RttStats* QuicSentPacketManager::GetRttStats() const {
  return &rtt_stats_;
}

QuicBandwidth QuicSentPacketManager::BandwidthEstimate() const {
  return send_algorithm_->BandwidthEstimate();
}

bool QuicSentPacketManager::HasReliableBandwidthEstimate() const {
  return send_algorithm_->HasReliableBandwidthEstimate();
}

const QuicSustainedBandwidthRecorder&
QuicSentPacketManager::SustainedBandwidthRecorder() const {
  return sustained_bandwidth_recorder_;
}

QuicByteCount QuicSentPacketManager::GetCongestionWindow() const {
  return send_algorithm_->GetCongestionWindow();
}

QuicByteCount QuicSentPacketManager::GetSlowStartThreshold() const {
  return send_algorithm_->GetSlowStartThreshold();
}

void QuicSentPacketManager::EnablePacing() {
  if (using_pacing_) {
    return;
  }

  // Set up a pacing sender with a 5 millisecond alarm granularity.
  using_pacing_ = true;
  send_algorithm_.reset(
      new PacingSender(send_algorithm_.release(),
                       QuicTime::Delta::FromMilliseconds(5),
                       kInitialUnpacedBurst));
}

}  // namespace net
