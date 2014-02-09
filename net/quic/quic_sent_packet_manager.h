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
#include "base/memory/scoped_ptr.h"
#include "net/base/linked_hash_map.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_ack_notifier_manager.h"
#include "net/quic/quic_protocol.h"

NET_EXPORT_PRIVATE extern bool FLAGS_track_retransmission_history;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_pacing;

namespace net {

namespace test {
class QuicConnectionPeer;
class QuicSentPacketManagerPeer;
}  // namespace test

class QuicClock;
class QuicConfig;
struct QuicConnectionStats;

// Class which tracks the set of packets sent on a QUIC connection and contains
// a send algorithm to decide when to send new packets.  It keeps track of any
// retransmittable data associated with each packet. If a packet is
// retransmitted, it will keep track of each version of a packet so that if a
// previous transmission is acked, the data will not be retransmitted.
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

  QuicSentPacketManager(bool is_server,
                        const QuicClock* clock,
                        QuicConnectionStats* stats,
                        CongestionFeedbackType congestion_type);
  virtual ~QuicSentPacketManager();

  virtual void SetFromConfig(const QuicConfig& config);

  virtual void SetMaxPacketSize(QuicByteCount max_packet_size);

  // Called when a new packet is serialized.  If the packet contains
  // retransmittable data, it will be added to the unacked packet map.
  void OnSerializedPacket(const SerializedPacket& serialized_packet);

  // Called when a packet is retransmitted with a new sequence number.
  // Replaces the old entry in the unacked packet map with the new
  // sequence number.
  void OnRetransmittedPacket(QuicPacketSequenceNumber old_sequence_number,
                             QuicPacketSequenceNumber new_sequence_number);

  // Processes the incoming ack and returns true if the retransmission or ack
  // alarm should be reset.
  bool OnIncomingAck(const ReceivedPacketInfo& received_info,
                     QuicTime ack_receive_time);

  // Discards any information for the packet corresponding to |sequence_number|.
  // If this packet has been retransmitted, information on those packets
  // will be discarded as well.  Also discards it from the congestion window if
  // it is present.
  void DiscardUnackedPacket(QuicPacketSequenceNumber sequence_number);

  // Returns true if the non-FEC packet |sequence_number| is unacked.
  bool IsUnacked(QuicPacketSequenceNumber sequence_number) const;

  // Requests retransmission of all unacked packets of |retransmission_type|.
  void RetransmitUnackedPackets(RetransmissionType retransmission_type);

  // Returns true if the unacked packet |sequence_number| has retransmittable
  // frames.  This will only return false if the packet has been acked, if a
  // previous transmission of this packet was ACK'd, or if this packet has been
  // retransmitted as with different sequence number.
  bool HasRetransmittableFrames(QuicPacketSequenceNumber sequence_number) const;

  // Returns true if there are pending retransmissions.
  bool HasPendingRetransmissions() const;

  // Retrieves the next pending retransmission.
  PendingRetransmission NextPendingRetransmission();

  bool HasUnackedPackets() const;

  // Returns the number of unacked packets which have retransmittable frames.
  size_t GetNumRetransmittablePackets() const;

  // Returns the smallest sequence number of a serialized packet which has not
  // been acked by the peer.  If there are no unacked packets, returns 0.
  QuicPacketSequenceNumber GetLeastUnackedSentPacket() const;

  // Returns the set of sequence numbers of all unacked packets.
  // Test only.
  SequenceNumberSet GetUnackedPackets() const;

  // Called when a congestion feedback frame is received from peer.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame,
      const QuicTime& feedback_receive_time);

  // Called when we have sent bytes to the peer.  This informs the manager both
  // the number of bytes sent and if they were retransmitted.  Returns true if
  // the sender should reset the retransmission timer.
  virtual bool OnPacketSent(QuicPacketSequenceNumber sequence_number,
                            QuicTime sent_time,
                            QuicByteCount bytes,
                            TransmissionType transmission_type,
                            HasRetransmittableData has_retransmittable_data);

  // Called when the retransmission timer expires.
  virtual void OnRetransmissionTimeout();

  // Calculate the time until we can send the next packet to the wire.
  // Note 1: When kUnknownWaitTime is returned, there is no need to poll
  // TimeUntilSend again until we receive an OnIncomingAckFrame event.
  // Note 2: Send algorithms may or may not use |retransmit| in their
  // calculations.
  virtual QuicTime::Delta TimeUntilSend(QuicTime now,
                                        TransmissionType transmission_type,
                                        HasRetransmittableData retransmittable,
                                        IsHandshake handshake);

  // Returns amount of time for delayed ack timer.
  const QuicTime::Delta DelayedAckTime() const;

  // Returns the current delay for the retransmission timer, which may send
  // either a tail loss probe or do a full RTO.  Returns QuicTime::Zero() if
  // there are no retransmittable packets.
  const QuicTime GetRetransmissionTime() const;

  // Returns the estimated smoothed RTT calculated by the congestion algorithm.
  const QuicTime::Delta SmoothedRtt() const;

  // Returns the estimated bandwidth calculated by the congestion algorithm.
  QuicBandwidth BandwidthEstimate() const;

  // Returns the size of the current congestion window in bytes.  Note, this is
  // not the *available* window.  Some send algorithms may not use a congestion
  // window and will return 0.
  QuicByteCount GetCongestionWindow() const;

  // Enables pacing if it has not already been enabled, and if
  // FLAGS_enable_quic_pacing is set.
  void MaybeEnablePacing();

  bool using_pacing() const { return using_pacing_; }


 private:
  friend class test::QuicConnectionPeer;
  friend class test::QuicSentPacketManagerPeer;

  enum ReceivedByPeer {
    RECEIVED_BY_PEER,
    NOT_RECEIVED_BY_PEER,
  };

  enum RetransmissionTimeoutMode {
    RTO_MODE,
    TLP_MODE,
    HANDSHAKE_MODE,
  };

  struct NET_EXPORT_PRIVATE TransmissionInfo {
    TransmissionInfo();

    // Constructs a Transmission with a new all_tranmissions set
    // containing |sequence_number|.
    TransmissionInfo(RetransmittableFrames* retransmittable_frames,
                     QuicPacketSequenceNumber sequence_number,
                     QuicSequenceNumberLength sequence_number_length);

    // Constructs a Transmission with the specified |all_tranmissions| set
    // and inserts |sequence_number| into it.
    TransmissionInfo(RetransmittableFrames* retransmittable_frames,
                     QuicPacketSequenceNumber sequence_number,
                     QuicSequenceNumberLength sequence_number_length,
                     SequenceNumberSet* all_transmissions);

    RetransmittableFrames* retransmittable_frames;
    QuicSequenceNumberLength sequence_number_length;
    // Zero when the packet is serialized, non-zero once it's sent.
    QuicTime sent_time;
    // Stores the sequence numbers of all transmissions of this packet.
    // Can never be null.
    SequenceNumberSet* all_transmissions;
    // Pending packets have not been abandoned or lost.
    bool pending;
  };

  typedef linked_hash_map<QuicPacketSequenceNumber,
                          TransmissionInfo> UnackedPacketMap;
  typedef linked_hash_map<QuicPacketSequenceNumber,
                          TransmissionType> PendingRetransmissionMap;

  static bool HasCryptoHandshake(const TransmissionInfo& transmission_info);

  // Returns true if there are unacked packets that are pending.
  bool HasPendingPackets() const;

  // Process the incoming ack looking for newly ack'd data packets.
  void HandleAckForSentPackets(const ReceivedPacketInfo& received_info);

  // Called when a packet is timed out, such as an RTO.  Removes the bytes from
  // the congestion manager, but does not change the congestion window size.
  virtual void OnPacketAbandoned(UnackedPacketMap::iterator it);

  // Returns the current retransmission mode.
  RetransmissionTimeoutMode GetRetransmissionMode() const;

  // Retransmits all crypto stream packets.
  void RetransmitCryptoPackets();

  // Retransmits the oldest pending packet.
  void RetransmitOldestPacket();

  // Retransmits all the packets and abandons by invoking a full RTO.
  void RetransmitAllPackets();

  // Returns the timer for retransmitting crypto handshake packets.
  const QuicTime::Delta GetCryptoRetransmissionDelay() const;

  // Returns the timer for a new tail loss probe.
  const QuicTime::Delta GetTailLossProbeDelay() const;

  // Returns the retransmission timeout, after which a full RTO occurs.
  const QuicTime::Delta GetRetransmissionDelay() const;

  // Update the RTT if the ack is for the largest acked sequence number.
  void MaybeUpdateRTT(const ReceivedPacketInfo& received_info,
                      const QuicTime& ack_receive_time);

  // Chooses whether to nack retransmit any packets based on the receipt info.
  // All acks have been handled before this method is invoked.
  void MaybeRetransmitOnAckFrame(const ReceivedPacketInfo& received_info,
                                 const QuicTime& ack_receive_time);

  // Marks |sequence_number| as being fully handled, either due to receipt
  // by the peer, or having been discarded as indecipherable.  Returns an
  // iterator to the next remaining unacked packet.
  UnackedPacketMap::iterator MarkPacketHandled(
      QuicPacketSequenceNumber sequence_number,
      ReceivedByPeer received_by_peer);

  // Removes entries from the unacked packet map.
  void RemovePacket(QuicPacketSequenceNumber sequence_number);

  // Request that |sequence_number| be retransmitted after the other pending
  // retransmissions.  Does not add it to the retransmissions if it's already
  // a pending retransmission.
  void MarkForRetransmission(QuicPacketSequenceNumber sequence_number,
                             TransmissionType transmission_type);

  // Clears up to |num_to_clear| previous transmissions in order to make room
  // in the ack frame for new acks.
  void ClearPreviousRetransmissions(size_t num_to_clear);

  void CleanupPacketHistory();

  // Newly serialized retransmittable and fec packets are added to this map,
  // which contains owning pointers to any contained frames.  If a packet is
  // retransmitted, this map will contain entries for both the old and the new
  // packet. The old packet's retransmittable frames entry will be NULL, while
  // the new packet's entry will contain the frames to retransmit.
  // If the old packet is acked before the new packet, then the old entry will
  // be removed from the map and the new entry's retransmittable frames will be
  // set to NULL.
  UnackedPacketMap unacked_packets_;

  // Pending retransmissions which have not been packetized and sent yet.
  PendingRetransmissionMap pending_retransmissions_;

  // Tracks if the connection was created by the server.
  bool is_server_;

  // An AckNotifier can register to be informed when ACKs have been received for
  // all packets that a given block of data was sent in. The AckNotifierManager
  // maintains the currently active notifiers.
  AckNotifierManager ack_notifier_manager_;

  const QuicClock* clock_;
  QuicConnectionStats* stats_;
  scoped_ptr<SendAlgorithmInterface> send_algorithm_;
  // Tracks the send time, size, and nack count of sent packets.  Packets are
  // removed after 5 seconds and they've been removed from pending_packets_.
  SendAlgorithmInterface::SentPacketsMap packet_history_map_;
  QuicTime::Delta rtt_sample_;  // RTT estimate from the most recent ACK.
  // Number of outstanding crypto handshake packets.
  size_t pending_crypto_packet_count_;
  // Number of times the RTO timer has fired in a row without receiving an ack.
  size_t consecutive_rto_count_;
  // Number of times the tail loss probe has been sent.
  size_t consecutive_tlp_count_;
  // Number of times the crypto handshake has been retransmitted.
  size_t consecutive_crypto_retransmission_count_;
  // Maximum number of tail loss probes to send before firing an RTO.
  size_t max_tail_loss_probes_;
  bool using_pacing_;

  DISALLOW_COPY_AND_ASSIGN(QuicSentPacketManager);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SENT_PACKET_MANAGER_H_
