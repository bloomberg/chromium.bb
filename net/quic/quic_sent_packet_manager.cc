// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/quic_ack_notifier_manager.h"

using std::make_pair;

// TODO(rtenneti): Remove this.
// Do not flip this flag until the flakiness of the
// net/tools/quic/end_to_end_test is fixed.
// If true, then QUIC connections will track the retransmission history of a
// packet so that an ack of a previous transmission will ack the data of all
// other transmissions.
bool FLAGS_track_retransmission_history = false;

namespace net {

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicSentPacketManager::HelperInterface::~HelperInterface() {
}

QuicSentPacketManager::QuicSentPacketManager(bool is_server,
                                             HelperInterface* helper)
    : is_server_(is_server),
      helper_(helper) {
}

QuicSentPacketManager::~QuicSentPacketManager() {
  STLDeleteValues(&unacked_packets_);
  while (!previous_transmissions_map_.empty()) {
    SequenceNumberSet* previous_transmissions =
        previous_transmissions_map_.begin()->second;
    for (SequenceNumberSet::const_iterator it = previous_transmissions->begin();
         it != previous_transmissions->end(); ++it) {
      DCHECK(ContainsKey(previous_transmissions_map_, *it));
      previous_transmissions_map_.erase(*it);
    }
    delete previous_transmissions;
  }
}

void QuicSentPacketManager::OnSerializedPacket(
    const SerializedPacket& serialized_packet, QuicTime serialized_time) {
  if (serialized_packet.packet->is_fec_packet()) {
    DCHECK(!serialized_packet.retransmittable_frames);
    unacked_fec_packets_.insert(make_pair(
        serialized_packet.sequence_number, serialized_time));
    return;
  }

  if (serialized_packet.retransmittable_frames == NULL) {
    // Don't track ack/congestion feedback packets.
    return;
  }

  ack_notifier_manager_.OnSerializedPacket(serialized_packet);

  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first <
         serialized_packet.sequence_number);
  unacked_packets_[serialized_packet.sequence_number] =
      serialized_packet.retransmittable_frames;
  retransmission_map_[serialized_packet.sequence_number] =
      RetransmissionInfo(serialized_packet.sequence_number,
                         serialized_packet.sequence_number_length);
}

void QuicSentPacketManager::OnRetransmittedPacket(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number) {
  DCHECK(ContainsKey(unacked_packets_, old_sequence_number));
  DCHECK(ContainsKey(retransmission_map_, old_sequence_number));
  DCHECK(ContainsKey(pending_retransmissions_, old_sequence_number));
  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < new_sequence_number);

  pending_retransmissions_.erase(old_sequence_number);

  RetransmissionInfo retransmission_info(
      new_sequence_number, GetSequenceNumberLength(old_sequence_number));
  retransmission_info.number_retransmissions =
      retransmission_map_[old_sequence_number].number_retransmissions + 1;
  retransmission_map_.erase(old_sequence_number);
  retransmission_map_[new_sequence_number] = retransmission_info;

  RetransmittableFrames* frames = unacked_packets_[old_sequence_number];
  DCHECK(frames);

  // A notifier may be waiting to hear about ACKs for the original sequence
  // number. Inform them that the sequence number has changed.
  ack_notifier_manager_.UpdateSequenceNumber(old_sequence_number,
                                             new_sequence_number);

  if (FLAGS_track_retransmission_history) {
    // We keep the old packet in the unacked packet list until it, or one of
    // the retransmissions of it are acked.
    unacked_packets_[old_sequence_number] = NULL;
  } else {
    unacked_packets_.erase(old_sequence_number);
  }
  unacked_packets_[new_sequence_number] = frames;

  if (!FLAGS_track_retransmission_history) {
    return;
  }
  // Keep track of all sequence numbers that this packet
  // has been transmitted as.
  SequenceNumberSet* previous_transmissions;
  PreviousTransmissionMap::iterator it =
      previous_transmissions_map_.find(old_sequence_number);
  if (it == previous_transmissions_map_.end()) {
    // This is the first retransmission of this packet, so create a new entry.
    previous_transmissions = new SequenceNumberSet;
    previous_transmissions_map_[old_sequence_number] = previous_transmissions;
    previous_transmissions->insert(old_sequence_number);
  } else {
    // Otherwise, use the existing entry.
    previous_transmissions = it->second;
  }
  previous_transmissions->insert(new_sequence_number);
  previous_transmissions_map_[new_sequence_number] = previous_transmissions;

  DCHECK(HasRetransmittableFrames(new_sequence_number));
}

void QuicSentPacketManager::OnIncomingAck(
    const ReceivedPacketInfo& received_info,
    bool is_truncated_ack) {
  HandleAckForSentPackets(received_info, is_truncated_ack);
  HandleAckForSentFecPackets(received_info);
}

void QuicSentPacketManager::DiscardUnackedPacket(
    QuicPacketSequenceNumber sequence_number) {
  MarkPacketReceivedByPeer(sequence_number);
}

void QuicSentPacketManager::HandleAckForSentPackets(
    const ReceivedPacketInfo& received_info,
    bool is_truncated_ack) {
  // Go through the packets we have not received an ack for and see if this
  // incoming_ack shows they've been seen by the peer.
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number > received_info.largest_observed) {
      // These are very new sequence_numbers.
      break;
    }

    if (!IsAwaitingPacket(received_info, sequence_number)) {
      // Packet was acked, so remove it from our unacked packet list.
      DVLOG(1) << ENDPOINT <<"Got an ack for packet " << sequence_number;
      // If data is associated with the most recent transmission of this
      // packet, then inform the caller.
      it = MarkPacketReceivedByPeer(sequence_number);

      // The AckNotifierManager is informed of every ACKed sequence number.
      ack_notifier_manager_.OnPacketAcked(sequence_number);

      continue;
    }

    // The peer got packets after this sequence number.  This is an explicit
    // nack.

    // TODO(rch): move this to the congestion manager, and fix the logic.
    // This is a packet which we planned on retransmitting and has not been
    // seen at the time of this ack being sent out.  See if it's our new
    // lowest unacked packet.
    DVLOG(1) << ENDPOINT << "still missing packet " << sequence_number;
    ++it;
    RetransmissionMap::iterator retransmission_it =
        retransmission_map_.find(sequence_number);
    if (retransmission_it == retransmission_map_.end()) {
      continue;
    }
    size_t nack_count = ++(retransmission_it->second.number_nacks);
    helper_->OnPacketNacked(sequence_number, nack_count);
  }

  // If we have received a trunacted ack, then we need to
  // clear out some previous transmissions to allow the peer
  // to actually ACK new packets.
  if (is_truncated_ack) {
    size_t num_to_clear = received_info.missing_packets.size() / 2;
    UnackedPacketMap::iterator it = unacked_packets_.begin();
    while (it != unacked_packets_.end() && num_to_clear > 0) {
      QuicPacketSequenceNumber sequence_number = it->first;
      ++it;
      // If this is not a previous transmission then there is no point
      // in clearing out any further packets, because it will not
      // affect the high water mark.
      if (!IsPreviousTransmission(sequence_number)) {
        break;
      }

      DCHECK(ContainsKey(previous_transmissions_map_, sequence_number));
      DCHECK(!ContainsKey(retransmission_map_, sequence_number));
      DCHECK(!HasRetransmittableFrames(sequence_number));
      unacked_packets_.erase(sequence_number);
      SequenceNumberSet* previous_transmissions =
          previous_transmissions_map_[sequence_number];
      previous_transmissions_map_.erase(sequence_number);
      previous_transmissions->erase(sequence_number);
      if (previous_transmissions->size() == 1) {
        previous_transmissions_map_.erase(*previous_transmissions->begin());
        delete previous_transmissions;
      }
      --num_to_clear;
    }
  }
}

bool QuicSentPacketManager::HasRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  if (!ContainsKey(unacked_packets_, sequence_number)) {
    return false;
  }

  return unacked_packets_.find(sequence_number)->second != NULL;
}

bool QuicSentPacketManager::MarkForRetransmission(
    QuicPacketSequenceNumber sequence_number,
    TransmissionType transmission_type) {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));
  if (!HasRetransmittableFrames(sequence_number)) {
    return false;
  }
  // If it's already in the retransmission map, don't add it again, just let
  // the prior retransmission request win out.
  if (ContainsKey(pending_retransmissions_, sequence_number)) {
    return true;
  }

  pending_retransmissions_[sequence_number] = transmission_type;
  return true;
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
  DCHECK(unacked_packets_[sequence_number]);

  return PendingRetransmission(sequence_number,
                               pending_retransmissions_.begin()->second,
                               GetRetransmittableFrames(sequence_number),
                               GetSequenceNumberLength(sequence_number));
}

bool QuicSentPacketManager::IsPreviousTransmission(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  PreviousTransmissionMap::const_iterator it =
      previous_transmissions_map_.find(sequence_number);
  if (it == previous_transmissions_map_.end()) {
    return false;
  }

  SequenceNumberSet* previous_transmissions = it->second;
  DCHECK(!previous_transmissions->empty());
  return *previous_transmissions->rbegin() != sequence_number;
}

QuicPacketSequenceNumber QuicSentPacketManager::GetMostRecentTransmission(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));

  PreviousTransmissionMap::const_iterator it =
      previous_transmissions_map_.find(sequence_number);
  if (it == previous_transmissions_map_.end()) {
    return sequence_number;
  }

  SequenceNumberSet* previous_transmissions =
      previous_transmissions_map_.find(sequence_number)->second;
  DCHECK(!previous_transmissions->empty());
  return *previous_transmissions->rbegin();
}

QuicSentPacketManager::UnackedPacketMap::iterator
QuicSentPacketManager::MarkPacketReceivedByPeer(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));
  UnackedPacketMap::iterator next_unacked =
      unacked_packets_.find(sequence_number);
  ++next_unacked;

  // If this packet has never been retransmitted, then simply drop it.
  if (!ContainsKey(previous_transmissions_map_, sequence_number)) {
    DiscardPacket(sequence_number);
    return next_unacked;
  }

  SequenceNumberSet* previous_transmissions =
    previous_transmissions_map_.find(sequence_number)->second;
  SequenceNumberSet::reverse_iterator previous_transmissions_it
      = previous_transmissions->rbegin();
  DCHECK(previous_transmissions_it != previous_transmissions->rend());

  QuicPacketSequenceNumber new_packet = *previous_transmissions_it;
  bool is_new_packet = new_packet == sequence_number;
  if (is_new_packet) {
    DiscardPacket(new_packet);
  } else {
    if (next_unacked->first == new_packet) {
      ++next_unacked;
    }
    // If we have received an ack for a previous transmission of a packet,
    // we want to keep the "new" transmission of the packet unacked,
    // but prevent the data from being retransmitted.
    delete unacked_packets_[new_packet];
    unacked_packets_[new_packet] = NULL;
    pending_retransmissions_.erase(new_packet);
  }
  previous_transmissions_map_.erase(new_packet);

  // Clear out information all previous transmissions.
  ++previous_transmissions_it;
  while (previous_transmissions_it != previous_transmissions->rend()) {
    QuicPacketSequenceNumber previous_transmission = *previous_transmissions_it;
    ++previous_transmissions_it;
    DCHECK(ContainsKey(previous_transmissions_map_, previous_transmission));
    previous_transmissions_map_.erase(previous_transmission);
    if (next_unacked != unacked_packets_.end() &&
        next_unacked->first == previous_transmission) {
      ++next_unacked;
    }
    DiscardPacket(previous_transmission);
  }

  delete previous_transmissions;

  next_unacked = unacked_packets_.begin();
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
    DCHECK(!ContainsKey(retransmission_map_, sequence_number));
    return;
  }

  // Delete the unacked packet.
  delete unacked_it->second;
  unacked_packets_.erase(unacked_it);
  retransmission_map_.erase(sequence_number);
  pending_retransmissions_.erase(sequence_number);
  return;
}

void QuicSentPacketManager::HandleAckForSentFecPackets(
    const ReceivedPacketInfo& received_info) {
  UnackedFecPacketMap::iterator it = unacked_fec_packets_.begin();
  while (it != unacked_fec_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number > received_info.largest_observed) {
      break;
    }

    if (!IsAwaitingPacket(received_info, sequence_number)) {
      DVLOG(1) << ENDPOINT << "Got an ack for fec packet: " << sequence_number;
      unacked_fec_packets_.erase(it++);
    } else {
      // TODO(rch): treat these packets more consistently.  They should
      // be subject to NACK and RTO based loss.  (Thought obviously, they
      // should not be retransmitted.)
      DVLOG(1) << ENDPOINT << "Still missing ack for fec packet: "
               << sequence_number;
      ++it;
    }
  }
}

void QuicSentPacketManager::DiscardFecPacket(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(ContainsKey(unacked_fec_packets_, sequence_number));
  unacked_fec_packets_.erase(sequence_number);
}

bool QuicSentPacketManager::IsRetransmission(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(HasRetransmittableFrames(sequence_number));
  RetransmissionMap::const_iterator it =
      retransmission_map_.find(sequence_number);
  return it != retransmission_map_.end() &&
      it->second.number_retransmissions > 0;
}

size_t QuicSentPacketManager::GetRetransmissionCount(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(HasRetransmittableFrames(sequence_number));
  DCHECK(ContainsKey(retransmission_map_, sequence_number));
  RetransmissionMap::const_iterator it =
      retransmission_map_.find(sequence_number);
  return it->second.number_retransmissions;
}

bool QuicSentPacketManager::IsUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return ContainsKey(unacked_packets_, sequence_number);
}

bool QuicSentPacketManager::IsFecUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return ContainsKey(unacked_fec_packets_, sequence_number);
}

const RetransmittableFrames& QuicSentPacketManager::GetRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));
  DCHECK(ContainsKey(retransmission_map_, sequence_number));

  return *unacked_packets_.find(sequence_number)->second;
}

QuicSequenceNumberLength QuicSentPacketManager::GetSequenceNumberLength(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_packets_, sequence_number));
  DCHECK(ContainsKey(retransmission_map_, sequence_number));

  return retransmission_map_.find(
      sequence_number)->second.sequence_number_length;
}

QuicTime QuicSentPacketManager::GetFecSentTime(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK(ContainsKey(unacked_fec_packets_, sequence_number));

  return unacked_fec_packets_.find(sequence_number)->second;
}

bool QuicSentPacketManager::HasUnackedPackets() const {
  return !unacked_packets_.empty();
}

size_t QuicSentPacketManager::GetNumUnackedPackets() const {
  return unacked_packets_.size();
}

bool QuicSentPacketManager::HasUnackedFecPackets() const {
  return !unacked_fec_packets_.empty();
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

QuicPacketSequenceNumber
QuicSentPacketManager::GetLeastUnackedFecPacket() const {
  if (unacked_fec_packets_.empty()) {
    // If there are no unacked packets, set the least unacked packet to
    // the sequence number of the next packet sent.
    return helper_->GetNextPacketSequenceNumber();
  }

  return unacked_fec_packets_.begin()->first;
}

SequenceNumberSet QuicSentPacketManager::GetUnackedPackets() const {
  SequenceNumberSet unacked_packets;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    unacked_packets.insert(it->first);
  }
  return unacked_packets;
}

}  // namespace net
