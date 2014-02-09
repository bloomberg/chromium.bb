// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/congestion_control/pacing_sender.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_ack_notifier_manager.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_utils_chromium.h"

using std::make_pair;
using std::max;
using std::min;

// TODO(rtenneti): Remove this.
// Do not flip this flag until the flakiness of the
// net/tools/quic/end_to_end_test is fixed.
// If true, then QUIC connections will track the retransmission history of a
// packet so that an ack of a previous transmission will ack the data of all
// other transmissions.
bool FLAGS_track_retransmission_history = false;

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

// TCP retransmits after 3 nacks.
static const size_t kNumberOfNacksBeforeRetransmission = 3;

// Only exponentially back off the handshake timer 5 times due to a timeout.
static const size_t kMaxHandshakeRetransmissionBackoffs = 5;
static const size_t kMinHandshakeTimeoutMs = 10;

// Sends up to two tail loss probes before firing an RTO,
// per draft RFC draft-dukkipati-tcpm-tcp-loss-probe.
static const size_t kDefaultMaxTailLossProbes = 2;
static const int64 kMinTailLossProbeTimeoutMs = 10;

COMPILE_ASSERT(kHistoryPeriodMs >= kBitrateSmoothingPeriodMs,
               history_must_be_longer_or_equal_to_the_smoothing_period);

}  // namespace

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")


QuicSentPacketManager::TransmissionInfo::TransmissionInfo()
    : retransmittable_frames(NULL),
      sequence_number_length(PACKET_1BYTE_SEQUENCE_NUMBER),
      sent_time(QuicTime::Zero()),
      all_transmissions(NULL),
      pending(false) { }

QuicSentPacketManager::TransmissionInfo::TransmissionInfo(
    RetransmittableFrames* retransmittable_frames,
    QuicPacketSequenceNumber sequence_number,
    QuicSequenceNumberLength sequence_number_length)
    : retransmittable_frames(retransmittable_frames),
      sequence_number_length(sequence_number_length),
      sent_time(QuicTime::Zero()),
      all_transmissions(new SequenceNumberSet),
      pending(false) {
  all_transmissions->insert(sequence_number);
}

QuicSentPacketManager::TransmissionInfo::TransmissionInfo(
    RetransmittableFrames* retransmittable_frames,
    QuicPacketSequenceNumber sequence_number,
    QuicSequenceNumberLength sequence_number_length,
    SequenceNumberSet* all_transmissions)
    : retransmittable_frames(retransmittable_frames),
      sequence_number_length(sequence_number_length),
      sent_time(QuicTime::Zero()),
      all_transmissions(all_transmissions),
      pending(false) {
  all_transmissions->insert(sequence_number);
}

QuicSentPacketManager::QuicSentPacketManager(bool is_server,
                                             const QuicClock* clock,
                                             QuicConnectionStats* stats,
                                             CongestionFeedbackType type)
    : is_server_(is_server),
      clock_(clock),
      stats_(stats),
      send_algorithm_(SendAlgorithmInterface::Create(clock, type)),
      rtt_sample_(QuicTime::Delta::Infinite()),
      pending_crypto_packet_count_(0),
      consecutive_rto_count_(0),
      consecutive_tlp_count_(0),
      consecutive_crypto_retransmission_count_(0),
      max_tail_loss_probes_(kDefaultMaxTailLossProbes),
      using_pacing_(false) {
}

QuicSentPacketManager::~QuicSentPacketManager() {
  for (UnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    delete it->second.retransmittable_frames;
    // Only delete all_transmissions once, for the newest packet.
    if (it->first == *it->second.all_transmissions->rbegin()) {
      delete it->second.all_transmissions;
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
    send_algorithm_->UpdateRtt(rtt_sample_);
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

    if (serialized_packet.retransmittable_frames->HasCryptoHandshake()
            == IS_HANDSHAKE) {
      ++pending_crypto_packet_count_;
    }
  }

  QuicPacketSequenceNumber sequence_number = serialized_packet.sequence_number;
  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < sequence_number);
  unacked_packets_[sequence_number] =
      TransmissionInfo(serialized_packet.retransmittable_frames,
                       serialized_packet.sequence_number,
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
  TransmissionInfo* transmission_info =
      FindOrNull(unacked_packets_, old_sequence_number);
  RetransmittableFrames* frames = transmission_info->retransmittable_frames;
  DCHECK(frames);

  // A notifier may be waiting to hear about ACKs for the original sequence
  // number. Inform them that the sequence number has changed.
  ack_notifier_manager_.UpdateSequenceNumber(old_sequence_number,
                                             new_sequence_number);

  // We keep the old packet in the unacked packet list until it, or one of
  // the retransmissions of it are acked.
  transmission_info->retransmittable_frames = NULL;
  unacked_packets_[new_sequence_number] =
      TransmissionInfo(frames, new_sequence_number,
                       transmission_info->sequence_number_length,
                       transmission_info->all_transmissions);
}

bool QuicSentPacketManager::OnIncomingAck(
    const ReceivedPacketInfo& received_info, QuicTime ack_receive_time) {
  // We rely on delta_time_largest_observed to compute an RTT estimate, so
  // we only update rtt when the largest observed gets acked.
  bool largest_observed_acked =
      ContainsKey(unacked_packets_, received_info.largest_observed);
  MaybeUpdateRTT(received_info, ack_receive_time);
  HandleAckForSentPackets(received_info);
  MaybeRetransmitOnAckFrame(received_info, ack_receive_time);

  // Anytime we are making forward progress and have a new RTT estimate, reset
  // the backoff counters.
  if (largest_observed_acked) {
    // Reset all retransmit counters any time a new packet is acked.
    consecutive_rto_count_ = 0;
    consecutive_tlp_count_ = 0;
    consecutive_crypto_retransmission_count_ = 0;
  }

  // Always reset the retransmission alarm when an ack comes in, since we now
  // have a better estimate of the current rtt than when it was set.
  return true;
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

  // Discard any retransmittable frames associated with revived packets.
  for (SequenceNumberSet::const_iterator revived_it =
           received_info.revived_packets.begin();
       revived_it != received_info.revived_packets.end(); ++revived_it) {
    TransmissionInfo* transmission_info =
        FindOrNull(unacked_packets_, *revived_it);
    if (transmission_info == NULL) {
      continue;
    }
    // The retransmittable frames are removed from the most recent transmission.
    transmission_info =
        FindOrNull(unacked_packets_,
                   *transmission_info->all_transmissions->rbegin());
    if (transmission_info->retransmittable_frames == NULL) {
      continue;
    }
    delete transmission_info->retransmittable_frames;
    transmission_info->retransmittable_frames = NULL;
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
    // If this is a pending packet, or has retransmittable data, then there is
    // no point in clearing out any further packets, because they would not
    // affect the high water mark.
    if (it->second.pending || it->second.retransmittable_frames != NULL) {
      break;
    }

    ++it;
    RemovePacket(sequence_number);
    --num_to_clear;
  }
}

bool QuicSentPacketManager::HasRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  const TransmissionInfo* transmission_info =
      FindOrNull(unacked_packets_, sequence_number);
  if (transmission_info == NULL) {
    return false;
  }

  return transmission_info->retransmittable_frames != NULL;
}

void QuicSentPacketManager::RetransmitUnackedPackets(
    RetransmissionType retransmission_type) {
  UnackedPacketMap::iterator unacked_it = unacked_packets_.begin();
  while (unacked_it != unacked_packets_.end()) {
    const RetransmittableFrames* frames =
        unacked_it->second.retransmittable_frames;
    // Only mark it as handled if it can't be retransmitted and there are no
    // pending retransmissions which would be cleared.
    if (frames == NULL && unacked_it->second.all_transmissions->size() == 1 &&
        retransmission_type == ALL_PACKETS) {
      unacked_it = MarkPacketHandled(unacked_it->first, NOT_RECEIVED_BY_PEER);
      continue;
    }
    // If it had no other transmissions, we handle it above.  If it has
    // other transmissions, one of them must have retransmittable frames,
    // so that gets resolved the same way as other retransmissions.
    // TODO(ianswett): Consider adding a new retransmission type which removes
    // all these old packets from unacked and retransmits them as new sequence
    // numbers with no connection to the previous ones.
    if (frames != NULL && (retransmission_type == ALL_PACKETS ||
                           frames->encryption_level() == ENCRYPTION_INITIAL)) {
      OnPacketAbandoned(unacked_it);
      MarkForRetransmission(unacked_it->first, NACK_RETRANSMISSION);
    }
    ++unacked_it;
  }
}

void QuicSentPacketManager::MarkForRetransmission(
    QuicPacketSequenceNumber sequence_number,
    TransmissionType transmission_type) {
  TransmissionInfo* transmission_info =
      FindOrNull(unacked_packets_, sequence_number);
  if (transmission_info != NULL) {
    LOG_IF(DFATAL, transmission_info->retransmittable_frames == NULL);
    LOG_IF(DFATAL, transmission_info->sent_time == QuicTime::Zero());
  } else {
    LOG(DFATAL) << "Unable to retransmit packet: " << sequence_number;
  }
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
  const TransmissionInfo& transmission_info =
      FindOrDie(unacked_packets_, sequence_number);
  DCHECK(transmission_info.retransmittable_frames);

  return PendingRetransmission(sequence_number,
                               pending_retransmissions_.begin()->second,
                               *transmission_info.retransmittable_frames,
                               transmission_info.sequence_number_length);
}

// static
bool QuicSentPacketManager::HasCryptoHandshake(
    const TransmissionInfo& transmission_info) {
  if (transmission_info.retransmittable_frames == NULL) {
    return false;
  }
  return transmission_info.retransmittable_frames->HasCryptoHandshake() ==
      IS_HANDSHAKE;
}

QuicSentPacketManager::UnackedPacketMap::iterator
QuicSentPacketManager::MarkPacketHandled(
    QuicPacketSequenceNumber sequence_number,
    ReceivedByPeer received_by_peer) {
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "Packet is not unacked: " << sequence_number;
    return it;
  }
  // If this packet is pending, remove it and inform the send algorithm.
  if (it->second.pending) {
    size_t bytes_sent = packet_history_map_[sequence_number]->bytes_sent();
    if (received_by_peer == RECEIVED_BY_PEER) {
      send_algorithm_->OnPacketAcked(sequence_number, bytes_sent);
    } else {
      // It's been abandoned.
      send_algorithm_->OnPacketAbandoned(sequence_number, bytes_sent);
    }
    it->second.pending = false;
  }

  SequenceNumberSet* all_transmissions = it->second.all_transmissions;
  DCHECK(!all_transmissions->empty());
  SequenceNumberSet::reverse_iterator all_transmissions_it =
      all_transmissions->rbegin();
  QuicPacketSequenceNumber newest_transmission = *all_transmissions_it;
  if (newest_transmission != sequence_number) {
    ++stats_->packets_spuriously_retransmitted;
  }

  bool has_cryto_handshake = HasCryptoHandshake(
      *FindOrNull(unacked_packets_, newest_transmission));
  if (has_cryto_handshake) {
    --pending_crypto_packet_count_;
  }
  while (all_transmissions_it != all_transmissions->rend()) {
    QuicPacketSequenceNumber previous_transmission = *all_transmissions_it;
    TransmissionInfo* transmission_info =
        FindOrNull(unacked_packets_, previous_transmission);
    if (transmission_info->retransmittable_frames != NULL) {
      // Since some version of this packet has been acked, ensure that
      // the data is not retransmitted again.
      delete transmission_info->retransmittable_frames;
      transmission_info->retransmittable_frames = NULL;
    }
    if (ContainsKey(pending_retransmissions_, previous_transmission)) {
      // Don't bother retransmitting this packet, if it has been
      // marked for retransmission.
      pending_retransmissions_.erase(previous_transmission);
    }
    if (has_cryto_handshake) {
      // If it's a crypto handshake packet, discard it and all retransmissions,
      // since they won't be acked now that one has been processed.
      if (transmission_info->pending) {
        OnPacketAbandoned(unacked_packets_.find(newest_transmission));
      }
      transmission_info->pending = false;
    }
    if (!transmission_info->pending) {
      unacked_packets_.erase(previous_transmission);
    } else {
      transmission_info->all_transmissions = new SequenceNumberSet;
      transmission_info->all_transmissions->insert(previous_transmission);
    }
    ++all_transmissions_it;
  }
  delete all_transmissions;

  UnackedPacketMap::iterator next_unacked = unacked_packets_.begin();
  while (next_unacked != unacked_packets_.end() &&
         next_unacked->first < sequence_number) {
    ++next_unacked;
  }
  return next_unacked;
}

void QuicSentPacketManager::RemovePacket(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "packet is not unacked: " << sequence_number;
    return;
  }
  const TransmissionInfo& transmission_info = it->second;
  transmission_info.all_transmissions->erase(sequence_number);
  if (transmission_info.all_transmissions->empty()) {
    delete transmission_info.all_transmissions;
  }
  unacked_packets_.erase(it);
}

bool QuicSentPacketManager::IsUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return ContainsKey(unacked_packets_, sequence_number);
}

bool QuicSentPacketManager::HasUnackedPackets() const {
  return !unacked_packets_.empty();
}

bool QuicSentPacketManager::HasPendingPackets() const {
  for (UnackedPacketMap::const_reverse_iterator it =
           unacked_packets_.rbegin(); it != unacked_packets_.rend(); ++it) {
    if (it->second.pending) {
      return true;
    }
  }
  return false;
}

size_t QuicSentPacketManager::GetNumRetransmittablePackets() const {
  size_t num_unacked_packets = 0;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    if (it->second.retransmittable_frames != NULL) {
      ++num_unacked_packets;
    }
  }
  return num_unacked_packets;
}

QuicPacketSequenceNumber
QuicSentPacketManager::GetLeastUnackedSentPacket() const {
  if (unacked_packets_.empty()) {
    // If there are no unacked packets, return 0.
    return 0;
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

bool QuicSentPacketManager::OnPacketSent(
    QuicPacketSequenceNumber sequence_number,
    QuicTime sent_time,
    QuicByteCount bytes,
    TransmissionType transmission_type,
    HasRetransmittableData has_retransmittable_data) {
  DCHECK_LT(0u, sequence_number);
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  // In rare circumstances, the packet could be serialized, sent, and then acked
  // before OnPacketSent is called.
  if (it == unacked_packets_.end()) {
    return false;
  }
  DCHECK(!it->second.pending);

  // Only track packets the send algorithm wants us to track.
  if (!send_algorithm_->OnPacketSent(sent_time, sequence_number, bytes,
                                     transmission_type,
                                     has_retransmittable_data)) {
    DCHECK(it->second.retransmittable_frames == NULL);
    RemovePacket(sequence_number);
    // Do not reset the retransmission timer, since the packet isn't tracked.
    return false;
  }

  const bool set_retransmission_timer = !HasPendingPackets();
  it->second.sent_time = sent_time;
  it->second.pending = true;
  packet_history_map_[sequence_number] =
      new SendAlgorithmInterface::SentPacket(bytes, sent_time);
  CleanupPacketHistory();

  // Reset the retransmission timer anytime a packet is sent in tail loss probe
  // mode or before the crypto handshake has completed.
  return set_retransmission_timer || GetRetransmissionMode() != RTO_MODE;
}

void QuicSentPacketManager::OnRetransmissionTimeout() {
  DCHECK(HasPendingPackets());
  // Handshake retransmission, TLP, and RTO are implemented with a single alarm.
  // The handshake alarm is set when the handshake has not completed, and the
  // TLP and RTO alarms are set after that.
  // The TLP alarm is always set to run for under an RTO.
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      ++stats_->crypto_retransmit_count;
      RetransmitCryptoPackets();
      return;
    case TLP_MODE:
      // If no tail loss probe can be sent, because there are no retransmittable
      // packets, execute a conventional RTO to abandon old packets.
      ++stats_->tlp_count;
      RetransmitOldestPacket();
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
  for (UnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = it->first;
    const RetransmittableFrames* frames = it->second.retransmittable_frames;
    // Only retransmit frames which are pending, and therefore have been sent.
    if (!it->second.pending || frames == NULL ||
        frames->HasCryptoHandshake() != IS_HANDSHAKE) {
      continue;
    }
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    packet_retransmitted = true;
    MarkForRetransmission(sequence_number, TLP_RETRANSMISSION);
    // Abandon all the crypto retransmissions now so they're not lost later.
    OnPacketAbandoned(it);
  }
  DCHECK(packet_retransmitted) << "No crypto packets found to retransmit.";
}

void QuicSentPacketManager::RetransmitOldestPacket() {
  DCHECK_EQ(TLP_MODE, GetRetransmissionMode());
  ++consecutive_tlp_count_;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = it->first;
    const RetransmittableFrames* frames = it->second.retransmittable_frames;
    // Only retransmit frames which are pending, and therefore have been sent.
    if (!it->second.pending || frames == NULL) {
      continue;
    }
    DCHECK_NE(IS_HANDSHAKE, frames->HasCryptoHandshake());
    MarkForRetransmission(sequence_number, TLP_RETRANSMISSION);
    return;
  }
  DLOG(FATAL)
    << "No retransmittable packets, so RetransmitOldestPacket failed.";
}

void QuicSentPacketManager::RetransmitAllPackets() {
  // Abandon all retransmittable packets and packets older than the
  // retransmission delay.

  DVLOG(1) << "OnRetransmissionTimeout() fired with "
           << unacked_packets_.size() << " unacked packets.";

  // Request retransmission of all retransmittable packets when the RTO
  // fires, and let the congestion manager decide how many to send
  // immediately and the remaining packets will be queued.
  // Abandon any non-retransmittable packets that are sufficiently old.
  bool packets_retransmitted = false;
  for (UnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    it->second.pending = false;
    if (it->second.retransmittable_frames != NULL) {
      packets_retransmitted = true;
      MarkForRetransmission(it->first, RTO_RETRANSMISSION);
    }
  }

  send_algorithm_->OnRetransmissionTimeout(packets_retransmitted);
  if (packets_retransmitted) {
    ++consecutive_rto_count_;
  }
}

QuicSentPacketManager::RetransmissionTimeoutMode
    QuicSentPacketManager::GetRetransmissionMode() const {
  DCHECK(HasPendingPackets());
  if (pending_crypto_packet_count_ > 0) {
    return HANDSHAKE_MODE;
  }
  if (consecutive_tlp_count_ < max_tail_loss_probes_) {
    // Ensure there are retransmittable frames.
    for (UnackedPacketMap::const_reverse_iterator it =
             unacked_packets_.rbegin(); it != unacked_packets_.rend(); ++it) {
      if (it->second.pending && it->second.retransmittable_frames) {
        return TLP_MODE;
      }
    }
  }
  return RTO_MODE;
}

void QuicSentPacketManager::OnPacketAbandoned(UnackedPacketMap::iterator it) {
  DCHECK(it != unacked_packets_.end());
  QuicPacketSequenceNumber sequence_number = it->first;
  DCHECK(ContainsKey(packet_history_map_, sequence_number));
  if (it->second.pending) {
    send_algorithm_->OnPacketAbandoned(
        sequence_number, packet_history_map_[sequence_number]->bytes_sent());
    it->second.pending = false;
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
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end() &&
         it->first <= received_info.largest_observed) {
    if (!it->second.pending) {
      ++it;
      continue;
    }
    QuicPacketSequenceNumber sequence_number = it->first;
    DVLOG(1) << "still missing packet " << sequence_number;
    // Acks must be handled previously, so ensure it's missing and not acked.
    DCHECK(IsAwaitingPacket(received_info, sequence_number));
    DCHECK(ContainsKey(packet_history_map_, sequence_number));
    const TransmissionInfo& transmission_info = it->second;
    SendAlgorithmInterface::SentPacket* sent_packet =
        packet_history_map_[sequence_number];

    // Consider it multiple nacks when there is a gap between the missing packet
    // and the largest observed, since the purpose of a nack threshold is to
    // tolerate re-ordering.  This handles both StretchAcks and Forward Acks.
    // TODO(ianswett): This relies heavily on sequential reception of packets,
    // and makes an assumption that the congestion control uses TCP style nacks.
    size_t min_nacks = received_info.largest_observed - sequence_number;
    sent_packet->Nack(min_nacks);

    size_t num_nacks_needed = kNumberOfNacksBeforeRetransmission;
    // Check for early retransmit(RFC5827) when the last packet gets acked and
    // the there are fewer than 4 pending packets.
    // TODO(ianswett): Set a retransmission timer instead of losing the packet
    // and retransmitting immediately.  Also consider only invoking OnPacketLost
    // and OnPacketAbandoned when they're actually retransmitted in case they
    // arrive while queued for retransmission.
    if (transmission_info.retransmittable_frames &&
        packet_history_map_.rbegin()->first == received_info.largest_observed) {
      num_nacks_needed = received_info.largest_observed - sequence_number;
    }

    if (sent_packet->nack_count() < num_nacks_needed) {
      ++it;
      continue;
    }

    // TODO(ianswett): If it's expected the FEC packet may repair the loss, it
    // should be recorded as a loss to the send algorithm, but not retransmitted
    // until it's known whether the FEC packet arrived.
    ++stats_->packets_lost;
    send_algorithm_->OnPacketLost(sequence_number, ack_receive_time);
    OnPacketAbandoned(it);

    if (transmission_info.retransmittable_frames) {
      MarkForRetransmission(sequence_number, NACK_RETRANSMISSION);
      ++it;
    } else {
      // Since we will not retransmit this, we need to remove it from
      // unacked_packets_.   This is either the current transmission of
      // a packet whose previous transmission has been acked, or it
      // is a packet that has been TLP retransmitted.
      ++it;
      RemovePacket(sequence_number);
    }
  }
}

void QuicSentPacketManager::MaybeUpdateRTT(
    const ReceivedPacketInfo& received_info,
    const QuicTime& ack_receive_time) {
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  const TransmissionInfo* transmission_info =
      FindOrNull(unacked_packets_, received_info.largest_observed);
  if (transmission_info == NULL) {
    return;
  }
  // Don't update the RTT if it hasn't been sent.
  if (transmission_info->sent_time == QuicTime::Zero()) {
    return;
  }

  QuicTime::Delta send_delta =
      ack_receive_time.Subtract(transmission_info->sent_time);
  if (send_delta > received_info.delta_time_largest_observed) {
    rtt_sample_ = send_delta.Subtract(
        received_info.delta_time_largest_observed);
  } else if (rtt_sample_.IsInfinite()) {
    // Even though we received information from the peer suggesting
    // an invalid (negative) RTT, we can use the send delta as an
    // approximation until we get a better estimate.
    rtt_sample_ = send_delta;
  }
  send_algorithm_->UpdateRtt(rtt_sample_);
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
const QuicTime::Delta QuicSentPacketManager::DelayedAckTime() const {
  return QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs/2);
}

const QuicTime QuicSentPacketManager::GetRetransmissionTime() const {
  // Don't set the timer if there are no pending packets.
  if (!HasPendingPackets()) {
    return QuicTime::Zero();
  }
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      return clock_->ApproximateNow().Add(GetCryptoRetransmissionDelay());
    case TLP_MODE: {
      // TODO(ianswett): When CWND is available, it would be preferable to
      // set the timer based on the earliest retransmittable packet.
      // Base the updated timer on the send time of the last packet.
      UnackedPacketMap::const_reverse_iterator it = unacked_packets_.rbegin();
      while (it != unacked_packets_.rend() &&
             (!it->second.pending ||
              it->second.retransmittable_frames == NULL)) {
        ++it;
      }
      DCHECK(it != unacked_packets_.rend());
      const QuicTime& sent_time = it->second.sent_time;
      const QuicTime tlp_time = sent_time.Add(GetTailLossProbeDelay());
      // Ensure the tlp timer never gets set to a time in the past.
      return QuicTime::Max(clock_->ApproximateNow(), tlp_time);
    }
    case RTO_MODE: {
      // The RTO is based on the first pending packet.
      UnackedPacketMap::const_iterator it = unacked_packets_.begin();
      while (it != unacked_packets_.end() && !it->second.pending) {
        ++it;
      }
      DCHECK(it != unacked_packets_.end());
      const QuicTime& sent_time = it->second.sent_time;
      // Always wait at least 1.5 * RTT after the first sent packet.
      QuicTime min_timeout = clock_->ApproximateNow().Add(
          SmoothedRtt().Multiply(1.5));
      QuicTime rto_timeout = sent_time.Add(GetRetransmissionDelay());

      return QuicTime::Max(min_timeout, rto_timeout);
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
                              1.5 * SmoothedRtt().ToMilliseconds());
  return QuicTime::Delta::FromMilliseconds(
      delay_ms << consecutive_crypto_retransmission_count_);
}

const QuicTime::Delta QuicSentPacketManager::GetTailLossProbeDelay() const {
  QuicTime::Delta srtt = SmoothedRtt();
  size_t num_pending = 0;
  for (UnackedPacketMap::const_reverse_iterator it = unacked_packets_.rbegin();
       it != unacked_packets_.rend(); ++it) {
    if (it->second.pending) {
      ++num_pending;
      if (num_pending > 1) {
        break;
      }
    }
  }
  DCHECK_LT(0u, num_pending);
  if (num_pending == 1) {
    return QuicTime::Delta::Max(
        srtt.Multiply(1.5).Add(DelayedAckTime()), srtt.Multiply(2));
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
    if (ContainsKey(unacked_packets_, history_it->first)) {
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
