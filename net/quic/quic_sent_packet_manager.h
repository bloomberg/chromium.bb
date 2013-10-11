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
#include "net/quic/quic_ack_notifier_manager.h"
#include "net/quic/quic_protocol.h"

NET_EXPORT_PRIVATE extern bool FLAGS_track_retransmission_history;

namespace net {

// Class which tracks the set of packets sent on a QUIC connection.
// It keeps track of the retransmittable data ssociated with each
// packets. If a packet is retransmitted, it will keep track of each
// version of a packet so that if a previous transmission is acked,
// the data will not be retransmitted.
class NET_EXPORT_PRIVATE QuicSentPacketManager {
 public:
  // Struct to store the pending retransmission information.
  struct PendingRetransmission {
    PendingRetransmission(QuicPacketSequenceNumber sequence_number,
                          TransmissionType transmission_type,
                          const RetransmittableFrames& retransmittable_frames,
                          QuicSequenceNumberLength sequence_number_length)
            : sequence_number(sequence_number),
              transmission_type(transmission_type),
              retransmittable_frames(retransmittable_frames),
              sequence_number_length(sequence_number_length) {
        }

        QuicPacketSequenceNumber sequence_number;
        TransmissionType transmission_type;
        const RetransmittableFrames& retransmittable_frames;
        QuicSequenceNumberLength sequence_number_length;
  };

  // Interface which provides callbacks that the manager needs.
  class NET_EXPORT_PRIVATE HelperInterface {
   public:
    virtual ~HelperInterface();

    // Called to return the sequence number of the next packet to be sent.
    virtual QuicPacketSequenceNumber GetNextPacketSequenceNumber() = 0;

    // Called when a packet has been explicitly NACK'd.  If a packet
    // has been retransmitted with mutliple sequence numbers, this will
    // only be called for the sequence number (if any) associated with
    // retransmittable frames.
    virtual void OnPacketNacked(QuicPacketSequenceNumber sequence_number,
                                size_t nack_count) = 0;
  };

  QuicSentPacketManager(bool is_server, HelperInterface* helper);
  virtual ~QuicSentPacketManager();

  // Called when a new packet is serialized.  If the packet contains
  // retransmittable data, it will be added to the unacked packet map.
  void OnSerializedPacket(const SerializedPacket& serialized_packet,
                          QuicTime serialized_time);

  // Called when a packet is retransmitted with a new sequence number.
  // Replaces the old entry in the unacked packet map with the new
  // sequence number.
  void OnRetransmittedPacket(QuicPacketSequenceNumber old_sequence_number,
                             QuicPacketSequenceNumber new_sequence_number);

  // Processes the ReceivedPacketInfo data from the incoming ack.
  void OnIncomingAck(const ReceivedPacketInfo& received_info,
                     bool is_truncated_ack);

  // Discards any information for the packet corresponding to |sequence_number|.
  // If this packet has been retransmitted, information on those packets
  // will be discarded as well.
  void DiscardUnackedPacket(QuicPacketSequenceNumber sequence_number);

  // Discards all information about fec packet |sequence_number|.
  void DiscardFecPacket(QuicPacketSequenceNumber sequence_number);

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

  // Returns true if the unacked packet |sequence_number| has retransmittable
  // frames.  This will only return false if the packet has been acked, if a
  // previous transmission of this packet was ACK'd, or if this packet has been
  // retransmitted as with different sequence number.
  bool HasRetransmittableFrames(QuicPacketSequenceNumber sequence_number) const;

  // Returns the RetransmittableFrames for |sequence_number|.
  const RetransmittableFrames& GetRetransmittableFrames(
      QuicPacketSequenceNumber sequence_number) const;

  // Request that |sequence_number| be retransmitted after the other pending
  // retransmissions.  Returns false if there are no retransmittable frames for
  // |sequence_number| and true if it will be retransmitted.
  bool MarkForRetransmission(QuicPacketSequenceNumber sequence_number,
                             TransmissionType transmission_type);

  // Returns true if there are pending retransmissions.
  bool HasPendingRetransmissions() const;

  // Retrieves the next pending retransmission.
  PendingRetransmission NextPendingRetransmission();

  // Returns the time the fec packet was sent.
  QuicTime GetFecSentTime(QuicPacketSequenceNumber sequence_number) const;

  // Returns true if there are any unacked packets.
  bool HasUnackedPackets() const;

  // Returns the number of unacked packets.
  size_t GetNumUnackedPackets() const;

  // Returns true if there are any unacked FEC packets.
  bool HasUnackedFecPackets() const;

  // Returns the smallest sequence number of a sent packet which has not been
  // acked by the peer.  Excludes any packets which have been retransmitted
  // with a new sequence number. If all packets have been acked, returns the
  // sequence number of the next packet that will be sent.
  QuicPacketSequenceNumber GetLeastUnackedSentPacket() const;

  // Returns the smallest sequence number of a sent fec packet which has not
  // been acked by the peer.  If all packets have been acked, returns the
  // sequence number of the next packet that will be sent.
  QuicPacketSequenceNumber GetLeastUnackedFecPacket() const;

  // Returns the set of sequence numbers of all unacked packets.
  SequenceNumberSet GetUnackedPackets() const;

  // Returns true if |sequence_number| is a previous transmission of packet.
  bool IsPreviousTransmission(QuicPacketSequenceNumber sequence_number) const;

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
    // TODO(ianswett): I believe this is now obsolete, or could at least be
    // changed to a bool.
    size_t number_retransmissions;
  };

  typedef linked_hash_map<QuicPacketSequenceNumber,
                          RetransmittableFrames*> UnackedPacketMap;
  typedef linked_hash_map<QuicPacketSequenceNumber,
                          QuicTime> UnackedFecPacketMap;
  typedef linked_hash_map<QuicPacketSequenceNumber,
                          TransmissionType> PendingRetransmissionMap;
  typedef base::hash_map<QuicPacketSequenceNumber,
                         RetransmissionInfo> RetransmissionMap;
  typedef base::hash_map<QuicPacketSequenceNumber, SequenceNumberSet*>
                         PreviousTransmissionMap;

  // Process the incoming ack looking for newly ack'd data packets.
  void HandleAckForSentPackets(const ReceivedPacketInfo& received_info,
                               bool is_truncated_ack);

  // Process the incoming ack looking for newly ack'd FEC packets.
  void HandleAckForSentFecPackets(const ReceivedPacketInfo& received_info);

  // Marks |sequence_number| as having been seen by the peer.  Returns an
  // iterator to the next remaining unacked packet.
  UnackedPacketMap::iterator MarkPacketReceivedByPeer(
      QuicPacketSequenceNumber sequence_number);

  // Simply removes the entries, if any, from the unacked packet map
  // and the retransmission map.
  void DiscardPacket(QuicPacketSequenceNumber sequence_number);

  // Returns the length of the serialized sequence number for
  // the packet |sequence_number|.
  QuicSequenceNumberLength GetSequenceNumberLength(
      QuicPacketSequenceNumber sequence_number) const;

  // Returns the sequence number of the packet that |sequence_number| was
  // most recently transmitted as.
  QuicPacketSequenceNumber GetMostRecentTransmission(
      QuicPacketSequenceNumber sequence_number) const;

  // When new packets are created which may be retransmitted, they are added
  // to this map, which contains owning pointers to the contained frames.  If
  // a packet is retransmitted, this map will contain entries for both the old
  // and the new packet.  The old packet's retransmittable frames entry will be
  // NULL, while the new packet's entry will contain the frames to retransmit.
  // If the old packet is acked before the new packet, then the old entry will
  // be removed from the map and the new entry's retransmittable frames will be
  // set to NULL.
  UnackedPacketMap unacked_packets_;

  // Pending fec packets that have not been acked yet. These packets need to be
  // cleared out of the cgst_window after a timeout since FEC packets are never
  // retransmitted.
  UnackedFecPacketMap unacked_fec_packets_;

  // Pending retransmissions which have not been packetized and sent yet.
  PendingRetransmissionMap pending_retransmissions_;

  // Map from sequence number to the retransmission info for a packet.
  // This includes the retransmission timeout, and the NACK count.  Only
  // the new transmission of a packet will have entries in this map.
  RetransmissionMap retransmission_map_;

  // Map from sequence number to set of all sequence number that this packet has
  // been transmitted as.  If a packet has not been retransmitted, it will not
  // have an entry in this map.  If any transmission of a packet has been acked
  // it will not have an entry in this map.
  PreviousTransmissionMap previous_transmissions_map_;

  // Tracks if the connection was created by the server.
  bool is_server_;

  HelperInterface* helper_;

  // An AckNotifier can register to be informed when ACKs have been received for
  // all packets that a given block of data was sent in. The AckNotifierManager
  // maintains the currently active notifiers.
  AckNotifierManager ack_notifier_manager_;

  DISALLOW_COPY_AND_ASSIGN(QuicSentPacketManager);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SENT_PACKET_MANAGER_H_
