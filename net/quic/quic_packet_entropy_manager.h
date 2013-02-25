// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Manages the packet entropy calculation for both sent and received packets
// for a connection.

#ifndef NET_QUIC_QUIC_PACKET_ENTROPY_MANAGER_H_
#define NET_QUIC_QUIC_PACKET_ENTROPY_MANAGER_H_

#include "net/base/linked_hash_map.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

// Records all sent and received packets by a connection to track the cumulative
// entropy of both sent and received packets separately. It is used by the
// connection to validate an ack frame sent by the peer as a preventive measure
// against the optimistic ack attack. Also, called by the framer when it
// truncates an ack frame to get the correct entropy value for the ack frame
// being serialized.
class NET_EXPORT_PRIVATE QuicPacketEntropyManager :
    public QuicReceivedEntropyHashCalculatorInterface {
 public:
  QuicPacketEntropyManager();
  virtual ~QuicPacketEntropyManager();

  // Record the received entropy hash against |sequence_number|.
  void RecordReceivedPacketEntropyHash(QuicPacketSequenceNumber sequence_number,
                                       QuicPacketEntropyHash entropy_hash);

  // Record |entropy_hash| for sent packet corresponding to |sequence_number|.
  void RecordSentPacketEntropyHash(QuicPacketSequenceNumber sequence_number,
                                   QuicPacketEntropyHash entropy_hash);

  // QuicReceivedEntropyHashCalculatorInterface
  // Called by QuicFramer, when the outgoing ack gets truncated, to recalculate
  // the received entropy hash for the truncated ack frame.
  virtual QuicPacketEntropyHash ReceivedEntropyHash(
      QuicPacketSequenceNumber sequence_number) const OVERRIDE;

  QuicPacketEntropyHash SentEntropyHash(
      QuicPacketSequenceNumber sequence_number) const;

  // Recalculate the received entropy hash since we had some missing packets
  // which the sender won't retransmit again and has sent us the |entropy_hash|
  // for packets up to, but not including, |sequence_number|.
  void RecalculateReceivedEntropyHash(
      QuicPacketSequenceNumber sequence_number,
      QuicPacketEntropyHash entropy_hash);

  // Returns true if |entropy_hash| matches the expected sent entropy hash
  // up to |sequence_number| removing sequence numbers from |missing_packets|.
  bool IsValidEntropy(QuicPacketSequenceNumber sequence_number,
                      const SequenceNumberSet& missing_packets,
                      QuicPacketEntropyHash entropy_hash) const;

  // Removes not required entries from |sent_packets_entropy_| before
  // |sequence_number|.
  void ClearSentEntropyBefore(QuicPacketSequenceNumber sequence_number);

  // Removes not required entries from |received_packets_entropy_| before
  // |sequence_number|.
  void ClearReceivedEntropyBefore(QuicPacketSequenceNumber sequence_number);

  QuicPacketEntropyHash sent_packets_entropy_hash() const {
    return sent_packets_entropy_hash_;
  }

  QuicPacketEntropyHash received_packets_entropy_hash() const {
    return received_packets_entropy_hash_;
  }

 private:
  typedef linked_hash_map<QuicPacketSequenceNumber,
                          std::pair<QuicPacketEntropyHash,
                               QuicPacketEntropyHash> > SentEntropyMap;
  typedef std::map<QuicPacketSequenceNumber,
                   QuicPacketEntropyHash> ReceivedEntropyMap;

  // TODO(satyamshekhar): Can be optimized using an interval set like data
  // structure.
  // Set of received sequence numbers that had the received entropy flag set.
  ReceivedEntropyMap received_packets_entropy_;

  // Linked hash map from sequence numbers to the sent entropy hash up to the
  // sequence number in the key.
  SentEntropyMap sent_packets_entropy_;

  // Cumulative hash of entropy of all sent packets.
  QuicPacketEntropyHash sent_packets_entropy_hash_;

  // Cumulative hash of entropy of all received packets.
  QuicPacketEntropyHash received_packets_entropy_hash_;

  QuicPacketSequenceNumber largest_received_sequence_number_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PACKET_ENTROPY_MANAGER_H_
