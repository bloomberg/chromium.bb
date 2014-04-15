// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Manages the packet entropy calculation for both sent and received packets
// for a connection.

#ifndef NET_QUIC_QUIC_RECEIVED_PACKET_MANAGER_H_
#define NET_QUIC_QUIC_RECEIVED_PACKET_MANAGER_H_

#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace test {
class QuicConnectionPeer;
class QuicReceivedPacketManagerPeer;
}  // namespace test

struct QuicConnectionStats;

// Records all received packets by a connection and tracks their entropy.
// Also calculates the correct entropy for the framer when it truncates an ack
// frame being serialized.
class NET_EXPORT_PRIVATE QuicReceivedPacketManager :
    public QuicReceivedEntropyHashCalculatorInterface {
 public:
  explicit QuicReceivedPacketManager(CongestionFeedbackType congestion_type,
                                     QuicConnectionStats* stats);
  virtual ~QuicReceivedPacketManager();

  // Updates the internal state concerning which packets have been received.
  // bytes: the packet size in bytes including Quic Headers.
  // header: the packet header.
  // timestamp: the arrival time of the packet.
  void RecordPacketReceived(QuicByteCount bytes,
                            const QuicPacketHeader& header,
                            QuicTime receipt_time);

  void RecordPacketRevived(QuicPacketSequenceNumber sequence_number);

  // Checks whether |sequence_number| is missing and less than largest observed.
  bool IsMissing(QuicPacketSequenceNumber sequence_number);

  // Checks if we're still waiting for the packet with |sequence_number|.
  bool IsAwaitingPacket(QuicPacketSequenceNumber sequence_number);

  // Update the |received_info| for an outgoing ack.
  void UpdateReceivedPacketInfo(ReceivedPacketInfo* received_info,
                                QuicTime approximate_now);

  // Should be called before sending an ACK packet, to decide if we need
  // to attach a QuicCongestionFeedbackFrame block.
  // Returns false if no QuicCongestionFeedbackFrame block is needed.
  // Otherwise fills in feedback and returns true.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback);

  // QuicReceivedEntropyHashCalculatorInterface
  // Called by QuicFramer, when the outgoing ack gets truncated, to recalculate
  // the received entropy hash for the truncated ack frame.
  virtual QuicPacketEntropyHash EntropyHash(
      QuicPacketSequenceNumber sequence_number) const OVERRIDE;

  // Updates internal state based on |received_info|.
  void UpdatePacketInformationReceivedByPeer(
      const ReceivedPacketInfo& received_nfo);
  // Updates internal state based on |stop_waiting|.
  void UpdatePacketInformationSentByPeer(
      const QuicStopWaitingFrame& stop_waiting);

  // Returns whether the peer is missing packets.
  bool HasMissingPackets();

  // Returns true when there are new missing packets to be reported within 3
  // packets of the largest observed.
  bool HasNewMissingPackets();

  QuicPacketSequenceNumber peer_largest_observed_packet() {
    return peer_largest_observed_packet_;
  }

  QuicPacketSequenceNumber least_packet_awaited_by_peer() {
    return least_packet_awaited_by_peer_;
  }

  QuicPacketSequenceNumber peer_least_packet_awaiting_ack() {
    return peer_least_packet_awaiting_ack_;
  }

 private:
  friend class test::QuicConnectionPeer;
  friend class test::QuicReceivedPacketManagerPeer;

  typedef std::map<QuicPacketSequenceNumber,
                   QuicPacketEntropyHash> ReceivedEntropyMap;

  // Record the received entropy hash against |sequence_number|.
  void RecordPacketEntropyHash(QuicPacketSequenceNumber sequence_number,
                               QuicPacketEntropyHash entropy_hash);

  // Recalculate the entropy hash and clears old packet entropies,
  // now that the sender sent us the |entropy_hash| for packets up to,
  // but not including, |peer_least_unacked|.
  void RecalculateEntropyHash(QuicPacketSequenceNumber peer_least_unacked,
                              QuicPacketEntropyHash entropy_hash);

  // Deletes all missing packets before least unacked. The connection won't
  // process any packets with sequence number before |least_unacked| that it
  // received after this call. Returns true if there were missing packets before
  // |least_unacked| unacked, false otherwise.
  bool DontWaitForPacketsBefore(QuicPacketSequenceNumber least_unacked);

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


  // Track some peer state so we can do less bookkeeping.
  // Largest sequence number that the peer has observed. Mostly received,
  // missing in case of truncated acks.
  QuicPacketSequenceNumber peer_largest_observed_packet_;
  // Least sequence number which the peer is still waiting for.
  QuicPacketSequenceNumber least_packet_awaited_by_peer_;
  // Least sequence number of the the packet sent by the peer for which it
  // hasn't received an ack.
  QuicPacketSequenceNumber peer_least_packet_awaiting_ack_;

  // Received packet information used to produce acks.
  ReceivedPacketInfo received_info_;

  // The time we received the largest_observed sequence number, or zero if
  // no sequence numbers have been received since UpdateReceivedPacketInfo.
  // Needed for calculating delta_time_largest_observed.
  QuicTime time_largest_observed_;

  scoped_ptr<ReceiveAlgorithmInterface> receive_algorithm_;

  QuicConnectionStats* stats_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_RECEIVED_PACKET_MANAGER_H_
