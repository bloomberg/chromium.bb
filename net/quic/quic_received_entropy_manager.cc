// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_received_entropy_manager.h"

#include "base/logging.h"
#include "net/base/linked_hash_map.h"

using std::make_pair;
using std::max;
using std::min;

namespace net {

QuicReceivedEntropyManager::QuicReceivedEntropyManager()
    : packets_entropy_hash_(0),
      largest_sequence_number_(0) {}

QuicReceivedEntropyManager::~QuicReceivedEntropyManager() {}

QuicPacketSequenceNumber
QuicReceivedEntropyManager::LargestSequenceNumber() const {
  if (packets_entropy_.empty()) {
    return 0;
  }
  return packets_entropy_.rbegin()->first;
}

void QuicReceivedEntropyManager::RecordPacketEntropyHash(
    QuicPacketSequenceNumber sequence_number,
    QuicPacketEntropyHash entropy_hash) {
  if (sequence_number < largest_sequence_number_) {
    DLOG(INFO) << "Ignoring received packet entropy for sequence_number:"
               << sequence_number << " less than largest_peer_sequence_number:"
               << largest_sequence_number_;
    return;
  }
  packets_entropy_.insert(make_pair(sequence_number, entropy_hash));
  packets_entropy_hash_ ^= entropy_hash;
  DVLOG(2) << "setting cumulative received entropy hash to: "
           << static_cast<int>(packets_entropy_hash_)
           << " updated with sequence number " << sequence_number
           << " entropy hash: " << static_cast<int>(entropy_hash);
}

QuicPacketEntropyHash QuicReceivedEntropyManager::EntropyHash(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK_LE(sequence_number, LargestSequenceNumber());
  DCHECK_GE(sequence_number, largest_sequence_number_);
  if (sequence_number == LargestSequenceNumber()) {
    return packets_entropy_hash_;
  }

  ReceivedEntropyMap::const_iterator it =
      packets_entropy_.upper_bound(sequence_number);
  // When this map is empty we should only query entropy for
  // |largest_received_sequence_number_|.
  LOG_IF(WARNING, it != packets_entropy_.end())
      << "largest_received: " << LargestSequenceNumber()
      << " sequence_number: " << sequence_number;

  // TODO(satyamshekhar): Make this O(1).
  QuicPacketEntropyHash hash = packets_entropy_hash_;
  for (; it != packets_entropy_.end(); ++it) {
    hash ^= it->second;
  }
  return hash;
}

void QuicReceivedEntropyManager::RecalculateEntropyHash(
    QuicPacketSequenceNumber peer_least_unacked,
    QuicPacketEntropyHash entropy_hash) {
  DLOG_IF(WARNING, peer_least_unacked > LargestSequenceNumber())
      << "Prematurely updating the entropy manager before registering the "
      << "entropy of the containing packet creates a temporary inconsistency.";
  if (peer_least_unacked < largest_sequence_number_) {
    DLOG(INFO) << "Ignoring received peer_least_unacked:" << peer_least_unacked
               << " less than largest_peer_sequence_number:"
               << largest_sequence_number_;
    return;
  }
  largest_sequence_number_ = peer_least_unacked;
  packets_entropy_hash_ = entropy_hash;
  ReceivedEntropyMap::iterator it =
      packets_entropy_.lower_bound(peer_least_unacked);
  // TODO(satyamshekhar): Make this O(1).
  for (; it != packets_entropy_.end(); ++it) {
    packets_entropy_hash_ ^= it->second;
  }
  // Discard entropies before least unacked.
  packets_entropy_.erase(
      packets_entropy_.begin(),
      packets_entropy_.lower_bound(
          min(peer_least_unacked, LargestSequenceNumber())));
}

}  // namespace net
