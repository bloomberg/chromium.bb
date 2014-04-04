// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_unacked_packet_map.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_utils_chromium.h"

using std::max;

namespace net {

QuicUnackedPacketMap::TransmissionInfo::TransmissionInfo()
    : retransmittable_frames(NULL),
      sequence_number_length(PACKET_1BYTE_SEQUENCE_NUMBER),
      sent_time(QuicTime::Zero()),
      bytes_sent(0),
      nack_count(0),
      all_transmissions(NULL),
      pending(false) { }

QuicUnackedPacketMap::TransmissionInfo::TransmissionInfo(
    RetransmittableFrames* retransmittable_frames,
    QuicPacketSequenceNumber sequence_number,
    QuicSequenceNumberLength sequence_number_length)
    : retransmittable_frames(retransmittable_frames),
      sequence_number_length(sequence_number_length),
      sent_time(QuicTime::Zero()),
      bytes_sent(0),
      nack_count(0),
      all_transmissions(new SequenceNumberSet),
      pending(false) {
  all_transmissions->insert(sequence_number);
}

QuicUnackedPacketMap::TransmissionInfo::TransmissionInfo(
    RetransmittableFrames* retransmittable_frames,
    QuicPacketSequenceNumber sequence_number,
    QuicSequenceNumberLength sequence_number_length,
    SequenceNumberSet* all_transmissions)
    : retransmittable_frames(retransmittable_frames),
      sequence_number_length(sequence_number_length),
      sent_time(QuicTime::Zero()),
      bytes_sent(0),
      nack_count(0),
      all_transmissions(all_transmissions),
      pending(false) {
  all_transmissions->insert(sequence_number);
}

QuicUnackedPacketMap::QuicUnackedPacketMap()
    : largest_sent_packet_(0),
      bytes_in_flight_(0),
      pending_crypto_packet_count_(0) {
}

QuicUnackedPacketMap::~QuicUnackedPacketMap() {
  for (UnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    delete it->second.retransmittable_frames;
    // Only delete all_transmissions once, for the newest packet.
    if (it->first == *it->second.all_transmissions->rbegin()) {
      delete it->second.all_transmissions;
    }
  }
}

// TODO(ianswett): Combine this method with OnPacketSent once packets are always
// sent in order and the connection tracks RetransmittableFrames for longer.
void QuicUnackedPacketMap::AddPacket(
    const SerializedPacket& serialized_packet) {
  if (!unacked_packets_.empty()) {
    bool is_old_packet = unacked_packets_.rbegin()->first >=
        serialized_packet.sequence_number;
    LOG_IF(DFATAL, is_old_packet) << "Old packet serialized: "
                                  << serialized_packet.sequence_number
                                  << " vs: "
                                  << unacked_packets_.rbegin()->first;
  }

  unacked_packets_[serialized_packet.sequence_number] =
      TransmissionInfo(serialized_packet.retransmittable_frames,
                       serialized_packet.sequence_number,
                       serialized_packet.sequence_number_length);
  if (serialized_packet.retransmittable_frames != NULL &&
      serialized_packet.retransmittable_frames->HasCryptoHandshake()
          == IS_HANDSHAKE) {
    ++pending_crypto_packet_count_;
  }
}

void QuicUnackedPacketMap::OnRetransmittedPacket(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number) {
  DCHECK(ContainsKey(unacked_packets_, old_sequence_number));
  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < new_sequence_number);

  // TODO(ianswett): Discard and lose the packet lazily instead of immediately.
  TransmissionInfo* transmission_info =
      FindOrNull(unacked_packets_, old_sequence_number);
  RetransmittableFrames* frames = transmission_info->retransmittable_frames;
  LOG_IF(DFATAL, frames == NULL) << "Attempt to retransmit packet with no "
                                 << "retransmittable frames: "
                                 << old_sequence_number;

  // We keep the old packet in the unacked packet list until it, or one of
  // the retransmissions of it are acked.
  transmission_info->retransmittable_frames = NULL;
  unacked_packets_[new_sequence_number] =
      TransmissionInfo(frames,
                       new_sequence_number,
                       transmission_info->sequence_number_length,
                       transmission_info->all_transmissions);
}

void QuicUnackedPacketMap::ClearPreviousRetransmissions(size_t num_to_clear) {
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

bool QuicUnackedPacketMap::HasRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  const TransmissionInfo* transmission_info =
      FindOrNull(unacked_packets_, sequence_number);
  if (transmission_info == NULL) {
    return false;
  }

  return transmission_info->retransmittable_frames != NULL;
}

void QuicUnackedPacketMap::NackPacket(QuicPacketSequenceNumber sequence_number,
                                      size_t min_nacks) {
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "NackPacket called for packet that is not unacked: "
                << sequence_number;
    return;
  }

  it->second.nack_count = max(min_nacks, it->second.nack_count);
}

void QuicUnackedPacketMap::RemovePacket(
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
  if (transmission_info.retransmittable_frames != NULL) {
    if (transmission_info.retransmittable_frames->HasCryptoHandshake()
            == IS_HANDSHAKE) {
      --pending_crypto_packet_count_;
    }
    delete transmission_info.retransmittable_frames;
  }
  unacked_packets_.erase(it);
}

void QuicUnackedPacketMap::NeuterPacket(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "packet is not unacked: " << sequence_number;
    return;
  }
  TransmissionInfo* transmission_info = &it->second;
  if (transmission_info->all_transmissions->size() > 1) {
    transmission_info->all_transmissions->erase(sequence_number);
    transmission_info->all_transmissions = new SequenceNumberSet();
    transmission_info->all_transmissions->insert(sequence_number);
  }
  if (transmission_info->retransmittable_frames != NULL) {
    delete transmission_info->retransmittable_frames;
    transmission_info->retransmittable_frames = NULL;
  }
}

// static
bool QuicUnackedPacketMap::IsSentAndNotPending(
    const TransmissionInfo& transmission_info) {
  return !transmission_info.pending &&
      transmission_info.sent_time != QuicTime::Zero() &&
      transmission_info.bytes_sent == 0;
}

bool QuicUnackedPacketMap::IsUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return ContainsKey(unacked_packets_, sequence_number);
}

bool QuicUnackedPacketMap::IsPending(
    QuicPacketSequenceNumber sequence_number) const {
  const TransmissionInfo* transmission_info =
      FindOrNull(unacked_packets_, sequence_number);
  return transmission_info != NULL && transmission_info->pending;
}

void QuicUnackedPacketMap::SetNotPending(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "SetNotPending called for packet that is not unacked: "
                << sequence_number;
    return;
  }
  if (it->second.pending) {
    LOG_IF(DFATAL, bytes_in_flight_ < it->second.bytes_sent);
    bytes_in_flight_ -= it->second.bytes_sent;
    it->second.pending = false;
  }
}

bool QuicUnackedPacketMap::HasUnackedPackets() const {
  return !unacked_packets_.empty();
}

bool QuicUnackedPacketMap::HasPendingPackets() const {
  for (UnackedPacketMap::const_reverse_iterator it =
           unacked_packets_.rbegin(); it != unacked_packets_.rend(); ++it) {
    if (it->second.pending) {
      return true;
    }
  }
  return false;
}

const QuicUnackedPacketMap::TransmissionInfo&
    QuicUnackedPacketMap::GetTransmissionInfo(
        QuicPacketSequenceNumber sequence_number) const {
  return unacked_packets_.find(sequence_number)->second;
}

QuicTime QuicUnackedPacketMap::GetLastPacketSentTime() const {
  UnackedPacketMap::const_reverse_iterator it = unacked_packets_.rbegin();
  while (it != unacked_packets_.rend()) {
    if (it->second.pending) {
      LOG_IF(DFATAL, it->second.sent_time == QuicTime::Zero())
          << "Sent time can never be zero for a pending packet.";
      return it->second.sent_time;
    }
    ++it;
  }
  LOG(DFATAL) << "Unable to find sent time.  "
              << "This method is only intended when there are pending packets.";
  return QuicTime::Zero();
}

QuicTime QuicUnackedPacketMap::GetFirstPendingPacketSentTime() const {
  UnackedPacketMap::const_iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end() && !it->second.pending) {
    ++it;
  }
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "No pending packets";
    return QuicTime::Zero();
  }
  return it->second.sent_time;
}

size_t QuicUnackedPacketMap::GetNumUnackedPackets() const {
  return unacked_packets_.size();
}

bool QuicUnackedPacketMap::HasMultiplePendingPackets() const {
  size_t num_pending = 0;
  for (UnackedPacketMap::const_reverse_iterator it = unacked_packets_.rbegin();
       it != unacked_packets_.rend(); ++it) {
    if (it->second.pending) {
      ++num_pending;
    }
    if (num_pending > 1) {
      return true;
    }
  }
  return false;
}

bool QuicUnackedPacketMap::HasPendingCryptoPackets() const {
  return pending_crypto_packet_count_ > 0;
}

bool QuicUnackedPacketMap::HasUnackedRetransmittableFrames() const {
  for (UnackedPacketMap::const_reverse_iterator it =
           unacked_packets_.rbegin(); it != unacked_packets_.rend(); ++it) {
    if (it->second.pending && it->second.retransmittable_frames) {
      return true;
    }
  }
  return false;
}

size_t QuicUnackedPacketMap::GetNumRetransmittablePackets() const {
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
QuicUnackedPacketMap::GetLeastUnackedSentPacket() const {
  if (unacked_packets_.empty()) {
    // If there are no unacked packets, return 0.
    return 0;
  }

  return unacked_packets_.begin()->first;
}

SequenceNumberSet QuicUnackedPacketMap::GetUnackedPackets() const {
  SequenceNumberSet unacked_packets;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    unacked_packets.insert(it->first);
  }
  return unacked_packets;
}

void QuicUnackedPacketMap::SetSent(QuicPacketSequenceNumber sequence_number,
                                   QuicTime sent_time,
                                   QuicByteCount bytes_sent,
                                   bool set_pending) {
  DCHECK_LT(0u, sequence_number);
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "OnPacketSent called for packet that is not unacked: "
                << sequence_number;
    return;
  }
  DCHECK(!it->second.pending);

  largest_sent_packet_ = max(sequence_number, largest_sent_packet_);
  it->second.sent_time = sent_time;
  if (set_pending) {
    bytes_in_flight_ += bytes_sent;
    it->second.bytes_sent = bytes_sent;
    it->second.pending = true;
  }
}

}  // namespace net
