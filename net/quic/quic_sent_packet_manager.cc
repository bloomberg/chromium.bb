// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"

using std::make_pair;

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
}

void QuicSentPacketManager::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  if (serialized_packet.packet->is_fec_packet()) {
    unacked_fec_packets_.insert(make_pair(
        serialized_packet.sequence_number,
        serialized_packet.retransmittable_frames));
    return;
  }

  if (serialized_packet.retransmittable_frames == NULL) {
    // Don't track ack/congestion feedback packets.
    return;
  }

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
  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < new_sequence_number);

  RetransmissionInfo retransmission_info(
      new_sequence_number, GetSequenceNumberLength(old_sequence_number));
  retransmission_info.number_retransmissions =
      retransmission_map_[old_sequence_number].number_retransmissions + 1;
  retransmission_map_.erase(old_sequence_number);
  retransmission_map_[new_sequence_number] = retransmission_info;

  RetransmittableFrames* frames = unacked_packets_[old_sequence_number];
  DCHECK(frames);
  unacked_packets_.erase(old_sequence_number);
  unacked_packets_[new_sequence_number] = frames;
}

void QuicSentPacketManager::HandleAckForSentPackets(
    const QuicAckFrame& incoming_ack,
    SequenceNumberSet* acked_packets) {
  // Go through the packets we have not received an ack for and see if this
  // incoming_ack shows they've been seen by the peer.
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number > helper_->GetPeerLargestObservedPacket()) {
      // These are very new sequence_numbers.
      break;
    }
    RetransmittableFrames* unacked = it->second;
    if (!IsAwaitingPacket(incoming_ack.received_info, sequence_number)) {
      // Packet was acked, so remove it from our unacked packet list.
      DVLOG(1) << ENDPOINT <<"Got an ack for packet " << sequence_number;
      acked_packets->insert(sequence_number);
      delete unacked;
      unacked_packets_.erase(it++);
      retransmission_map_.erase(sequence_number);
    } else {
      // This is a packet which we planned on retransmitting and has not been
      // seen at the time of this ack being sent out.  See if it's our new
      // lowest unacked packet.
      DVLOG(1) << ENDPOINT << "still missing packet " << sequence_number;
      ++it;
      // The peer got packets after this sequence number.  This is an explicit
      // nack.
      RetransmissionMap::iterator retransmission_it =
          retransmission_map_.find(sequence_number);
      if (retransmission_it == retransmission_map_.end()) {
        continue;
      }
      size_t nack_count = ++(retransmission_it->second.number_nacks);
      helper_->OnPacketNacked(sequence_number, nack_count);
    }
  }
}

void QuicSentPacketManager::HandleAckForSentFecPackets(
    const QuicAckFrame& incoming_ack,
    SequenceNumberSet* acked_packets) {
  UnackedPacketMap::iterator it = unacked_fec_packets_.begin();
  while (it != unacked_fec_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number > helper_->GetPeerLargestObservedPacket()) {
      break;
    }
    if (!IsAwaitingPacket(incoming_ack.received_info, sequence_number)) {
      DVLOG(1) << ENDPOINT << "Got an ack for fec packet: " << sequence_number;
      acked_packets->insert(sequence_number);
      unacked_fec_packets_.erase(it++);
    } else {
      DVLOG(1) << ENDPOINT << "Still missing ack for fec packet: "
               << sequence_number;
      ++it;
    }
  }
}

void QuicSentPacketManager::DiscardPacket(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator unacked_it =
      unacked_packets_.find(sequence_number);
  if (unacked_it == unacked_packets_.end()) {
    // Packet was not meant to be retransmitted.
    DCHECK(!ContainsKey(retransmission_map_, sequence_number));
    return;
  }

  // Delete the unacked packet.
  delete unacked_it->second;
  unacked_packets_.erase(unacked_it);
  retransmission_map_.erase(sequence_number);
}

bool QuicSentPacketManager::IsRetransmission(
    QuicPacketSequenceNumber sequence_number) const {
  RetransmissionMap::const_iterator it =
      retransmission_map_.find(sequence_number);
  return it != retransmission_map_.end() &&
      it->second.number_retransmissions > 0;
}

size_t QuicSentPacketManager::GetRetransmissionCount(
    QuicPacketSequenceNumber sequence_number) const {
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

bool QuicSentPacketManager::HasUnackedPackets() const {
  return !unacked_packets_.empty();
}

size_t QuicSentPacketManager::GetNumUnackedPackets() const {
  return unacked_packets_.size();
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

}  // namespace net
