// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Manages the packet entropy calculation for both sent and received packets
// for a connection.

#ifndef NET_QUIC_QUIC_RECEIVED_ENTROPY_MANAGER_H_
#define NET_QUIC_QUIC_RECEIVED_ENTROPY_MANAGER_H_

#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

// Records all received packets by a connection to track the cumulative
// entropy of received packets.  Also, called by the framer when it truncates
// an ack frame to calculate the correct entropy value for the ack frame being
// serialized.
class NET_EXPORT_PRIVATE QuicReceivedEntropyManager :
    public QuicReceivedEntropyHashCalculatorInterface {
 public:
  QuicReceivedEntropyManager();
  virtual ~QuicReceivedEntropyManager();

  // Record the received entropy hash against |sequence_number|.
  void RecordPacketEntropyHash(QuicPacketSequenceNumber sequence_number,
                               QuicPacketEntropyHash entropy_hash);

  // QuicReceivedEntropyHashCalculatorInterface
  // Called by QuicFramer, when the outgoing ack gets truncated, to recalculate
  // the received entropy hash for the truncated ack frame.
  virtual QuicPacketEntropyHash EntropyHash(
      QuicPacketSequenceNumber sequence_number) const OVERRIDE;

  QuicPacketSequenceNumber LargestSequenceNumber() const;

  // Recalculate the entropy hash and clears old packet entropies,
  // now that the sender sent us the |entropy_hash| for packets up to,
  // but not including, |peer_least_unacked|.
  void RecalculateEntropyHash(QuicPacketSequenceNumber peer_least_unacked,
                              QuicPacketEntropyHash entropy_hash);

  QuicPacketEntropyHash packets_entropy_hash() const {
    return packets_entropy_hash_;
  }

 private:
  typedef std::map<QuicPacketSequenceNumber,
                   QuicPacketEntropyHash> ReceivedEntropyMap;

  // TODO(satyamshekhar): Can be optimized using an interval set like data
  // structure.
  // Map of received sequence numbers to their corresponding entropy.
  // Every received packet has an entry, and packets without the entropy bit set
  // have an entropy value of 0.
  // TODO(ianswett): When the entropy flag is off, the entropy should not be 0.
  ReceivedEntropyMap packets_entropy_;

  // Cumulative hash of entropy of all received packets.
  QuicPacketEntropyHash packets_entropy_hash_;

  // The largest sequence number cleared by RecalculateEntropyHash.
  // Received entropy cannot be calculated for numbers less than it.
  QuicPacketSequenceNumber largest_sequence_number_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_RECEIVED_ENTROPY_MANAGER_H_
