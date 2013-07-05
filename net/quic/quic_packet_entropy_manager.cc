// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_entropy_manager.h"

#include "base/logging.h"
#include "net/base/linked_hash_map.h"

using std::make_pair;
using std::max;
using std::min;

namespace net {

QuicPacketEntropyManager::QuicPacketEntropyManager()
    : sent_packets_entropy_hash_(0),
      received_packets_entropy_hash_(0),
      largest_received_sequence_number_(0) {}

QuicPacketEntropyManager::~QuicPacketEntropyManager() {}

QuicPacketSequenceNumber
QuicPacketEntropyManager::LargestReceivedSequenceNumber() const {
  if (received_packets_entropy_.empty()) {
    return 0;
  }
  return received_packets_entropy_.rbegin()->first;
}

void QuicPacketEntropyManager::RecordReceivedPacketEntropyHash(
    QuicPacketSequenceNumber sequence_number,
    QuicPacketEntropyHash entropy_hash) {
  if (sequence_number < largest_received_sequence_number_) {
    DLOG(INFO) << "Ignoring received packet entropy for sequence_number:"
               << sequence_number << " less than largest_peer_sequence_number:"
               << largest_received_sequence_number_;
    return;
  }
  received_packets_entropy_.insert(make_pair(sequence_number, entropy_hash));
  received_packets_entropy_hash_ ^= entropy_hash;
  DVLOG(2) << "setting cumulative received entropy hash to: "
           << static_cast<int>(received_packets_entropy_hash_)
           << " updated with sequence number " << sequence_number
           << " entropy hash: " << static_cast<int>(entropy_hash);
}

void QuicPacketEntropyManager::RecordSentPacketEntropyHash(
    QuicPacketSequenceNumber sequence_number,
    QuicPacketEntropyHash entropy_hash) {
  // TODO(satyamshekhar): Check this logic again when/if we enable packet
  // reordering.
  sent_packets_entropy_hash_ ^= entropy_hash;
  sent_packets_entropy_.insert(
      make_pair(sequence_number,
                make_pair(entropy_hash, sent_packets_entropy_hash_)));
  DVLOG(2) << "setting cumulative sent entropy hash to: "
           << static_cast<int>(sent_packets_entropy_hash_)
           << " updated with sequence number " << sequence_number
           << " entropy hash: " << static_cast<int>(entropy_hash);
}

QuicPacketEntropyHash QuicPacketEntropyManager::ReceivedEntropyHash(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK_LE(sequence_number, LargestReceivedSequenceNumber());
  DCHECK_GE(sequence_number, largest_received_sequence_number_);
  if (sequence_number == LargestReceivedSequenceNumber()) {
    return received_packets_entropy_hash_;
  }

  ReceivedEntropyMap::const_iterator it =
      received_packets_entropy_.upper_bound(sequence_number);
  // When this map is empty we should only query entropy for
  // |largest_received_sequence_number_|.
  LOG_IF(WARNING, it != received_packets_entropy_.end())
      << "largest_received: " << LargestReceivedSequenceNumber()
      << " sequence_number: " << sequence_number;

  // TODO(satyamshekhar): Make this O(1).
  QuicPacketEntropyHash hash = received_packets_entropy_hash_;
  for (; it != received_packets_entropy_.end(); ++it) {
    hash ^= it->second;
  }
  return hash;
}

QuicPacketEntropyHash QuicPacketEntropyManager::SentEntropyHash(
    QuicPacketSequenceNumber sequence_number) const {
  SentEntropyMap::const_iterator it =
      sent_packets_entropy_.find(sequence_number);
  if (it == sent_packets_entropy_.end()) {
    // Should only happen when we have not received ack for any packet.
    DCHECK_EQ(0u, sequence_number);
    return 0;
  }
  return it->second.second;
}

void QuicPacketEntropyManager::RecalculateReceivedEntropyHash(
    QuicPacketSequenceNumber peer_least_unacked,
    QuicPacketEntropyHash entropy_hash) {
  DLOG_IF(WARNING, peer_least_unacked > LargestReceivedSequenceNumber())
      << "Prematurely updating the entropy manager before registering the "
      << "entropy of the containing packet creates a temporary inconsistency.";
  if (peer_least_unacked < largest_received_sequence_number_) {
    DLOG(INFO) << "Ignoring received peer_least_unacked:" << peer_least_unacked
               << " less than largest_peer_sequence_number:"
               << largest_received_sequence_number_;
    return;
  }
  largest_received_sequence_number_ = peer_least_unacked;
  received_packets_entropy_hash_ = entropy_hash;
  ReceivedEntropyMap::iterator it =
      received_packets_entropy_.lower_bound(peer_least_unacked);
  // TODO(satyamshekhar): Make this O(1).
  for (; it != received_packets_entropy_.end(); ++it) {
    received_packets_entropy_hash_ ^= it->second;
  }
  // Discard entropies before least unacked.
  received_packets_entropy_.erase(
      received_packets_entropy_.begin(),
      received_packets_entropy_.lower_bound(
          min(peer_least_unacked, LargestReceivedSequenceNumber())));
}

bool QuicPacketEntropyManager::IsValidEntropy(
    QuicPacketSequenceNumber sequence_number,
    const SequenceNumberSet& missing_packets,
    QuicPacketEntropyHash entropy_hash) const {
  SentEntropyMap::const_iterator entropy_it =
      sent_packets_entropy_.find(sequence_number);
  if (entropy_it == sent_packets_entropy_.end()) {
    DCHECK_EQ(0u, sequence_number);
    // Close connection if something goes wrong.
    return 0 == sequence_number;
  }
  QuicPacketEntropyHash expected_entropy_hash = entropy_it->second.second;
  for (SequenceNumberSet::const_iterator it = missing_packets.begin();
       it != missing_packets.end(); ++it) {
    entropy_it = sent_packets_entropy_.find(*it);
    DCHECK(entropy_it != sent_packets_entropy_.end());
    expected_entropy_hash ^= entropy_it->second.first;
  }
  DLOG_IF(WARNING, entropy_hash != expected_entropy_hash)
      << "Invalid entropy hash: " << static_cast<int>(entropy_hash)
      << " expected entropy hash: " << static_cast<int>(expected_entropy_hash);
  return entropy_hash == expected_entropy_hash;
}

void QuicPacketEntropyManager::ClearSentEntropyBefore(
    QuicPacketSequenceNumber sequence_number) {
  if (sent_packets_entropy_.empty()) {
    return;
  }
  SentEntropyMap::iterator it = sent_packets_entropy_.begin();
  while (it->first < sequence_number) {
    sent_packets_entropy_.erase(it);
    it = sent_packets_entropy_.begin();
    DCHECK(it != sent_packets_entropy_.end());
  }
  DVLOG(2) << "Cleared entropy before: "
           << sent_packets_entropy_.begin()->first;
}

}  // namespace net
