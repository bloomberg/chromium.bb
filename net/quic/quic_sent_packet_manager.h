// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SENT_PACKET_MANAGER_H_
#define NET_QUIC_QUIC_SENT_PACKET_MANAGER_H_

#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "net/base/linked_hash_map.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE QuicSentPacketManager {
 public:
  class NET_EXPORT_PRIVATE HelperInterface {
   public:
    virtual QuicPacketSequenceNumber GetPeerLargestObservedPacket() = 0;
    virtual QuicPacketSequenceNumber GetNextPacketSequenceNumber() = 0;

    // Called when a packet has been explicitly NACKd
    virtual void OnPacketNacked(QuicPacketSequenceNumber sequence_number,
                                size_t nack_count) = 0;
    virtual ~HelperInterface();
  };

  QuicSentPacketManager(bool is_server, HelperInterface* helper);
  virtual ~QuicSentPacketManager();

  // Called when a new packet is serialized.  If the packet contains
  // retransmittable data, it will be added to the unacked packet map.
  void OnSerializedPacket(const SerializedPacket& serialized_packet);

  // Called when a packet is retransmitted with a new sequence number.
  // Replaces the old entry in the unacked packet map with the new
  // sequence number.
  void OnRetransmittedPacket(QuicPacketSequenceNumber old_sequence_number,
                             QuicPacketSequenceNumber new_sequence_number);

  // Process the incoming ack looking for newly ack'd data packets.
  void HandleAckForSentPackets(const QuicAckFrame& incoming_ack,
                               SequenceNumberSet* acked_packets);

  // Process the incoming ack looking for newly ack'd FEC packets.
  void HandleAckForSentFecPackets(const QuicAckFrame& incoming_ack,
                                  SequenceNumberSet* acked_packets);

  // Discards all information about packet |sequence_number|.
  void DiscardPacket(QuicPacketSequenceNumber sequence_number);

  // Returns true if |sequence_number| is a retransmission of a packet.
  bool IsRetransmission(QuicPacketSequenceNumber sequence_number) const;

  // Returns the number of times the data in the packet |sequence_number|
  // has been transmitted.
  size_t GetRetransmissionCount(
      QuicPacketSequenceNumber sequence_number) const;

  // Returns true if the non-FEC packet |sequence_number| is unacked.
  bool IsUnacked(QuicPacketSequenceNumber sequence_number) const;

  // Returns true if the FEC packet |sequence_number| is unacked.
  bool IsFecUnacked(QuicPacketSequenceNumber sequence_number) const;

  // Returns the RetransmittableFrames for |sequence_number|.
  const RetransmittableFrames& GetRetransmittableFrames(
      QuicPacketSequenceNumber sequence_number) const;

  // Returns the length of the serialized sequence number for
  // the packet |sequence_number|.
  QuicSequenceNumberLength GetSequenceNumberLength(
      QuicPacketSequenceNumber sequence_number) const;

  // Returns true if there are any unacked packets.
  bool HasUnackedPackets() const;

  // Returns the number of unacked packets.
  size_t GetNumUnackedPackets() const;

  // Returns the smallest sequence number of a sent packet which has not
  // been acked by the peer.  If all packets have been acked, returns the
  // sequence number of the next packet that will be sent.
  QuicPacketSequenceNumber GetLeastUnackedSentPacket() const;

  // Returns the set of unacked packet sequence numbers.
  SequenceNumberSet GetUnackedPackets() const;

 private:
  struct RetransmissionInfo {
    RetransmissionInfo() {}
    explicit RetransmissionInfo(QuicPacketSequenceNumber sequence_number,
                                QuicSequenceNumberLength sequence_number_length)
        : sequence_number(sequence_number),
          sequence_number_length(sequence_number_length),
          number_nacks(0),
          number_retransmissions(0) {
    }

    QuicPacketSequenceNumber sequence_number;
    QuicSequenceNumberLength sequence_number_length;
    size_t number_nacks;
    size_t number_retransmissions;
  };

  typedef linked_hash_map<QuicPacketSequenceNumber,
                          RetransmittableFrames*> UnackedPacketMap;
  typedef base::hash_map<QuicPacketSequenceNumber,
                         RetransmissionInfo> RetransmissionMap;

  // When new packets are created which may be retransmitted, they are added
  // to this map, which contains owning pointers to the contained frames.
  UnackedPacketMap unacked_packets_;

  // Pending fec packets that have not been acked yet. These packets need to be
  // cleared out of the cgst_window after a timeout since FEC packets are never
  // retransmitted.
  // TODO(satyamshekhar): What should be the timeout for these packets?
  UnackedPacketMap unacked_fec_packets_;

  // Map from sequence number to the retransmission info.
  RetransmissionMap retransmission_map_;

  // Tracks if the connection was created by the server.
  bool is_server_;
  HelperInterface* helper_;

  DISALLOW_COPY_AND_ASSIGN(QuicSentPacketManager);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SENT_PACKET_MANAGER_H_
