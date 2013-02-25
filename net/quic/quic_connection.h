// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The entity that handles framing writes for a Quic client or server.
// Each QuicSession will have a connection associated with it.
//
// On the server side, the Dispatcher handles the raw reads, and hands off
// packets via ProcessUdpPacket for framing and processing.
//
// On the client side, the Connection handles the raw reads, as well as the
// processing.
//
// Note: this class is not thread-safe.

#ifndef NET_QUIC_QUIC_CONNECTION_H_
#define NET_QUIC_QUIC_CONNECTION_H_

#include <list>
#include <map>
#include <queue>
#include <set>
#include <vector>

#include "base/hash_tables.h"
#include "net/base/ip_endpoint.h"
#include "net/base/linked_hash_map.h"
#include "net/quic/congestion_control/quic_congestion_manager.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_packet_entropy_manager.h"
#include "net/quic/quic_packet_generator.h"
#include "net/quic/quic_protocol.h"

namespace net {

class QuicClock;
class QuicConnection;
class QuicEncrypter;
class QuicFecGroup;
class QuicRandom;

namespace test {
class QuicConnectionPeer;
}  // namespace test

class NET_EXPORT_PRIVATE QuicConnectionVisitorInterface {
 public:
  virtual ~QuicConnectionVisitorInterface() {}

  // A simple visitor interface for dealing with data frames.  The session
  // should determine if all frames will be accepted, and return true if so.
  // If any frames can't be processed or buffered, none of the data should
  // be used, and the callee should return false.
  virtual bool OnPacket(const IPEndPoint& self_address,
                        const IPEndPoint& peer_address,
                        const QuicPacketHeader& header,
                        const std::vector<QuicStreamFrame>& frame) = 0;

  // Called when the stream is reset by the peer.
  virtual void OnRstStream(const QuicRstStreamFrame& frame) = 0;

  // Called when the connection is going away according to the peer.
  virtual void OnGoAway(const QuicGoAwayFrame& frame) = 0;

  // Called when the connection is closed either locally by the framer, or
  // remotely by the peer.
  virtual void ConnectionClose(QuicErrorCode error, bool from_peer) = 0;

  // Called when packets are acked by the peer.
  virtual void OnAck(const SequenceNumberSet& acked_packets) = 0;

  // Called when a blocked socket becomes writable.  If all pending bytes for
  // this visitor are consumed by the connection successfully this should
  // return true, otherwise it should return false.
  virtual bool OnCanWrite() = 0;
};

// Interface which gets callbacks from the QuicConnection at interesting
// points.  Implementations must not mutate the state of the connection
// as a result of these callbacks.
class NET_EXPORT_PRIVATE QuicConnectionDebugVisitorInterface {
 public:
  virtual ~QuicConnectionDebugVisitorInterface() {}

  // Called when a packet has been received, but before it is
  // validated or parsed.
  virtual void OnPacketReceived(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet) = 0;

  // Called when the header of a packet has been parsed.
  virtual void OnPacketHeader(const QuicPacketHeader& header) = 0;

  // Called when a StreamFrame has been parsed.
  virtual void OnStreamFrame(const QuicStreamFrame& frame) = 0;

  // Called when a AckFrame has been parsed.
  virtual void OnAckFrame(const QuicAckFrame& frame) = 0;

  // Called when a CongestionFeedbackFrame has been parsed.
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) = 0;

  // Called when a RstStreamFrame has been parsed.
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) = 0;

  // Called when a ConnectionCloseFrame has been parsed.
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) = 0;

  // Called when a public reset packet has been received.
  virtual void OnPublicResetPacket(const QuicPublicResetPacket& packet) = 0;

  // Called after a packet has been successfully parsed which results
  // in the revival of a packet via FEC.
  virtual void OnRevivedPacket(const QuicPacketHeader& revived_header,
                               base::StringPiece payload) = 0;
};

class NET_EXPORT_PRIVATE QuicConnectionHelperInterface {
 public:
  virtual ~QuicConnectionHelperInterface() {}

  // Sets the QuicConnection to be used by this helper.  This method
  // must only be called once.
  virtual void SetConnection(QuicConnection* connection) = 0;

  // Returns a QuicClock to be used for all time related functions.
  virtual const QuicClock* GetClock() const = 0;

  // Returns a QuicRandom to be used for all random number related functions.
  virtual QuicRandom* GetRandomGenerator() = 0;

  // Sends the packet out to the peer, possibly simulating packet
  // loss if FLAGS_fake_packet_loss_percentage is set.  If the write
  // succeeded, returns the number of bytes written.  If the write
  // failed, returns -1 and the error code will be copied to |*error|.
  virtual int WritePacketToWire(const QuicEncryptedPacket& packet,
                                int* error) = 0;

  // Sets up an alarm to retransmit a packet if we haven't received an ack
  // in the expected time frame.  Implementations must invoke
  // OnRetransmissionAlarm when the alarm fires.  Implementations must also
  // handle the case where |this| is deleted before the alarm fires.  If the
  // alarm is already set, this call is a no-op.
  virtual void SetRetransmissionAlarm(QuicTime::Delta delay) = 0;

  // Sets an alarm to send packets after |delay_in_us|.  Implementations must
  // invoke OnCanWrite when the alarm fires.  Implementations must also
  // handle the case where |this| is deleted before the alarm fires.
  virtual void SetSendAlarm(QuicTime::Delta delay) = 0;

  // Sets an alarm which fires when the connection may have timed out.
  // Implementations must call CheckForTimeout() and then reregister the alarm
  // if the connection has not yet timed out.
  virtual void SetTimeoutAlarm(QuicTime::Delta delay) = 0;

  // Returns true if a send alarm is currently set.
  virtual bool IsSendAlarmSet() = 0;

  // If a send alarm is currently set, this method unregisters it.  If
  // no send alarm is set, it does nothing.
  virtual void UnregisterSendAlarmIfRegistered() = 0;

  // Sets an alarm which fires when an Ack may need to be sent.
  // Implementations must call SendAck() when the alarm fires.
  // If the alarm is already registered for a shorter timeout, this call is a
  // no-op.
  virtual void SetAckAlarm(QuicTime::Delta delay) = 0;

  // Clears the ack alarm if it was set.  If it was not set, this is a no-op.
  virtual void ClearAckAlarm() = 0;
};

class NET_EXPORT_PRIVATE QuicConnection
    : public QuicFramerVisitorInterface,
      public QuicBlockedWriterInterface,
      public QuicPacketGenerator::DelegateInterface {
 public:
  // Constructs a new QuicConnection for the specified |guid| and |address|.
  // |helper| will be owned by this connection.
  QuicConnection(QuicGuid guid,
                 IPEndPoint address,
                 QuicConnectionHelperInterface* helper);
  virtual ~QuicConnection();

  static void DeleteEnclosedFrame(QuicFrame* frame);

  // Send the data payload to the peer.
  // Returns a pair with the number of bytes consumed from data, and a boolean
  // indicating if the fin bit was consumed.  This does not indicate the data
  // has been sent on the wire: it may have been turned into a packet and queued
  // if the socket was unexpectedly blocked.
  QuicConsumedData SendStreamData(QuicStreamId id,
                                  base::StringPiece data,
                                  QuicStreamOffset offset,
                                  bool fin);
  // Send a stream reset frame to the peer.
  virtual void SendRstStream(QuicStreamId id,
                             QuicErrorCode error);

  // Sends the connection close packet without affecting the state of the
  // connection.  This should only be called if the session is actively being
  // destroyed: otherwise call SendConnectionCloseWithDetails instead.
  virtual void SendConnectionClosePacket(QuicErrorCode error,
                                         const std::string& details);

  // Sends a connection close frame to the peer, and closes the connection by
  // calling CloseConnection(notifying the visitor as it does so).
  virtual void SendConnectionClose(QuicErrorCode error);
  virtual void SendConnectionCloseWithDetails(QuicErrorCode error,
                                              const std::string& details);
  // Notifies the visitor of the close and marks the connection as disconnected.
  void CloseConnection(QuicErrorCode error, bool from_peer);
  virtual void SendGoAway(QuicErrorCode error,
                          QuicStreamId last_good_stream_id,
                          const std::string& reason);

  // Processes an incoming UDP packet (consisting of a QuicEncryptedPacket) from
  // the peer.  If processing this packet permits a packet to be revived from
  // its FEC group that packet will be revived and processed.
  virtual void ProcessUdpPacket(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet);

  // QuicBlockedWriterInterface
  // Called when the underlying connection becomes writable to allow
  // queued writes to happen.  Returns false if the socket has become blocked.
  virtual bool OnCanWrite() OVERRIDE;

  // From QuicFramerVisitorInterface
  virtual void OnError(QuicFramer* framer) OVERRIDE;
  virtual void OnPacket() OVERRIDE;
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) OVERRIDE;
  virtual void OnRevivedPacket() OVERRIDE;
  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnFecProtectedPayload(base::StringPiece payload) OVERRIDE;
  virtual void OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE;
  virtual void OnAckFrame(const QuicAckFrame& frame) OVERRIDE;
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE;
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE;
  virtual void OnGoAwayFrame(const QuicGoAwayFrame& frame) OVERRIDE;
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) OVERRIDE;
  virtual void OnFecData(const QuicFecData& fec) OVERRIDE;
  virtual void OnPacketComplete() OVERRIDE;

  // QuicPacketGenerator::DelegateInterface
  virtual QuicAckFrame* CreateAckFrame() OVERRIDE;
  virtual QuicCongestionFeedbackFrame* CreateFeedbackFrame() OVERRIDE;
  virtual bool OnSerializedPacket(const SerializedPacket& packet) OVERRIDE;

  // Accessors
  void set_visitor(QuicConnectionVisitorInterface* visitor) {
    visitor_ = visitor;
  }
  void set_debug_visitor(QuicConnectionDebugVisitorInterface* debug_visitor) {
    debug_visitor_ = debug_visitor;
  }
  const IPEndPoint& self_address() const { return self_address_; }
  const IPEndPoint& peer_address() const { return peer_address_; }
  QuicGuid guid() const { return guid_; }
  const QuicClock* clock() const { return clock_; }
  QuicRandom* random_generator() const { return random_generator_; }

  // Updates the internal state concerning which packets have been acked.
  void RecordPacketReceived(const QuicPacketHeader& header);

  // Called by a RetransmissionAlarm when the timer goes off.  If the peer
  // appears to be sending truncated acks, this returns false to indicate
  // failure, otherwise it calls MaybeRetransmitPacket and returns true.
  bool MaybeRetransmitPacketForRTO(QuicPacketSequenceNumber sequence_number);

  // Called to retransmit a packet, in the case a packet was sufficiently
  // nacked by the peer, or not acked within the time out window.
  void RetransmitPacket(QuicPacketSequenceNumber sequence_number);

  QuicPacketCreator::Options* options() { return packet_creator_.options(); }

  bool connected() { return connected_; }

  size_t NumFecGroups() const { return group_map_.size(); }

  // Testing only.
  size_t NumQueuedPackets() const { return queued_packets_.size(); }

  // Returns true if the connection has queued packets or frames.
  bool HasQueuedData() const;

  // If the connection has timed out, this will close the connection and return
  // true.  Otherwise, it will return false and will reset the timeout alarm.
  bool CheckForTimeout();

  // Returns true of the next packet to be sent should be "lost" by
  // not actually writing it to the wire.
  bool ShouldSimulateLostPacket();

  // Sets up a packet with an QuicAckFrame and sends it out.
  void SendAck();

  // Called when an RTO fires.  Returns the time when this alarm
  // should next fire, or 0 if no retransmission alarm should be set.
  QuicTime OnRetransmissionTimeout();

 protected:
  // Deletes all missing packets before least unacked. The connection won't
  // process any packets with sequence number before |least_unacked| that it
  // received after this call. Returns true if there were missing packets before
  // |least_unacked| unacked, false otherwise.
  bool DontWaitForPacketsBefore(QuicPacketSequenceNumber least_unacked);

  // Send a packet to the peer. If |sequence_number| is present in the
  // |retransmission_map_|, then contents of this packet will be retransmitted
  // with a new sequence number if it's not acked by the peer. Deletes
  // |packet| via WritePacket call or transfers ownership to QueuedPacket,
  // ultimately deleted via WritePacket. Also, it updates the entropy map
  // corresponding to |sequence_number| using |entropy_hash|.
  // TODO(wtc): none of the callers check the return value.
  virtual bool SendOrQueuePacket(QuicPacketSequenceNumber sequence_number,
                                 QuicPacket* packet,
                                 QuicPacketEntropyHash entropy_hash);

  // Writes the given packet to socket with the help of helper. Returns true on
  // successful write, false otherwise. However, behavior is undefined if
  // connection is not established or broken. In any circumstances, a return
  // value of true implies that |packet| has been deleted and should not be
  // accessed. If |sequence_number| is present in |retransmission_map_| it also
  // sets up retransmission of the given packet in case of successful write. If
  // |force| is true, then the packet will be sent immediately and the send
  // scheduler will not be consulted.
  bool WritePacket(QuicPacketSequenceNumber sequence_number,
                   QuicPacket* packet,
                   bool force);

  // Make sure an ack we got from our peer is sane.
  bool ValidateAckFrame(const QuicAckFrame& incoming_ack);

  // These two are called by OnAckFrame to update the appropriate internal
  // state.
  //
  // Updates internal state based on incoming_ack.received_info
  void UpdatePacketInformationReceivedByPeer(
      const QuicAckFrame& incoming_ack);
  // Updates internal state based in incoming_ack.sent_info
  void UpdatePacketInformationSentByPeer(const QuicAckFrame& incoming_ack);

  QuicConnectionHelperInterface* helper() { return helper_; }

 private:
  friend class test::QuicConnectionPeer;

  // Packets which have not been written to the wire.
  // Owns the QuicPacket* packet.
  struct QueuedPacket {
    QueuedPacket(QuicPacketSequenceNumber sequence_number,
                 QuicPacket* packet)
        : sequence_number(sequence_number),
          packet(packet) {
    }

    QuicPacketSequenceNumber sequence_number;
    QuicPacket* packet;
  };

  struct RetransmissionInfo {
    explicit RetransmissionInfo(QuicPacketSequenceNumber sequence_number)
        : sequence_number(sequence_number),
          scheduled_time(QuicTime::Zero()),
          number_nacks(0),
          number_retransmissions(0) {
    }

    QuicPacketSequenceNumber sequence_number;
    QuicTime scheduled_time;
    size_t number_nacks;
    size_t number_retransmissions;
  };

  class RetransmissionInfoComparator {
   public:
    bool operator()(const RetransmissionInfo& lhs,
                    const RetransmissionInfo& rhs) const {
      DCHECK(lhs.scheduled_time.IsInitialized() &&
             rhs.scheduled_time.IsInitialized());
      return lhs.scheduled_time > rhs.scheduled_time;
    }
  };

  typedef std::list<QueuedPacket> QueuedPacketList;
  typedef linked_hash_map<QuicPacketSequenceNumber,
                          RetransmittableFrames*> UnackedPacketMap;
  typedef std::map<QuicFecGroupNumber, QuicFecGroup*> FecGroupMap;
  typedef base::hash_map<QuicPacketSequenceNumber,
                         RetransmissionInfo> RetransmissionMap;
  typedef std::priority_queue<RetransmissionInfo,
                              std::vector<RetransmissionInfo>,
                              RetransmissionInfoComparator>
      RetransmissionTimeouts;

  // Checks if a packet can be written now, and sets the timer if necessary.
  virtual bool CanWrite(bool is_retransmission) OVERRIDE;

  void MaybeSetupRetransmission(QuicPacketSequenceNumber sequence_number);
  bool IsRetransmission(QuicPacketSequenceNumber sequence_number);

  // Writes as many queued packets as possible.  The connection must not be
  // blocked when this is called.
  bool WriteQueuedPackets();

  // If a packet can be revived from the current FEC group, then
  // revive and process the packet.
  void MaybeProcessRevivedPacket();

  void UpdateOutgoingAck();

  void MaybeSendAckInResponseToPacket();

  // Get the FEC group associate with the last processed packet.
  QuicFecGroup* GetFecGroup();

  // Closes any FEC groups protecting packets before |sequence_number|.
  void CloseFecGroupsBefore(QuicPacketSequenceNumber sequence_number);

  QuicConnectionHelperInterface* helper_;
  QuicFramer framer_;
  const QuicClock* clock_;
  QuicRandom* random_generator_;

  const QuicGuid guid_;
  // Address on the last successfully processed packet received from the
  // client.
  IPEndPoint self_address_;
  IPEndPoint peer_address_;
  // Address on the last(currently being processed) packet received. Not
  // verified/authenticated.
  IPEndPoint last_self_address_;
  IPEndPoint last_peer_address_;

  bool last_packet_revived_;  // True if the last packet was revived from FEC.
  size_t last_size_;  // Size of the last received packet.
  QuicPacketHeader last_header_;
  std::vector<QuicStreamFrame> last_stream_frames_;

  QuicAckFrame outgoing_ack_;
  QuicCongestionFeedbackFrame outgoing_congestion_feedback_;

  // Track some peer state so we can do less bookkeeping
  // Largest sequence sent by the peer which had an ack frame (latest ack info).
  QuicPacketSequenceNumber largest_seen_packet_with_ack_;
  // Largest sequence number that the peer has observed. Mostly received,
  // missing in case of truncated acks.
  QuicPacketSequenceNumber peer_largest_observed_packet_;
  // Least sequence number which the peer is still waiting for.
  QuicPacketSequenceNumber least_packet_awaited_by_peer_;
  // Least sequence number of the the packet sent by the peer for which it
  // hasn't received an ack.
  QuicPacketSequenceNumber peer_least_packet_awaiting_ack_;

  // When new packets are created which may be retransmitted, they are added
  // to this map, which contains owning pointers to the contained frames.
  UnackedPacketMap unacked_packets_;

  // Heap of packets that we might need to retransmit, and the time at
  // which we should retransmit them. Every time a packet is sent it is added
  // to this heap which is O(log(number of pending packets to be retransmitted))
  // which might be costly. This should be optimized to O(1) by maintaining a
  // priority queue of lists of packets to be retransmitted, where list x
  // contains all packets that have been retransmitted x times.
  RetransmissionTimeouts retransmission_timeouts_;

  // Map from sequence number to the retransmission info.
  RetransmissionMap retransmission_map_;

  // True while OnRetransmissionTimeout is running to prevent
  // SetRetransmissionAlarm from being called erroneously.
  bool handling_retransmission_timeout_;

  // When packets could not be sent because the socket was not writable,
  // they are added to this list.  All corresponding frames are in
  // unacked_packets_ if they are to be retransmitted.
  QueuedPacketList queued_packets_;

  // True when the socket becomes unwritable.
  bool write_blocked_;

  FecGroupMap group_map_;

  QuicPacketEntropyManager entropy_manager_;

  QuicConnectionVisitorInterface* visitor_;
  QuicConnectionDebugVisitorInterface* debug_visitor_;
  QuicPacketCreator packet_creator_;
  QuicPacketGenerator packet_generator_;

  // Network idle time before we kill of this connection.
  const QuicTime::Delta timeout_;

  // The time that we got a packet for this connection.
  QuicTime time_of_last_received_packet_;

  // The time that we last sent a packet for this connection.
  QuicTime time_of_last_sent_packet_;

  // Congestion manager which controls the rate the connection sends packets
  // as well as collecting and generating congestion feedback.
  QuicCongestionManager congestion_manager_;

  // True by default.  False if we've received or sent an explicit connection
  // close.
  bool connected_;

  // True if the last ack received from the peer may have been truncated.  False
  // otherwise.
  bool received_truncated_ack_;

  bool send_ack_in_response_to_packet_;

  DISALLOW_COPY_AND_ASSIGN(QuicConnection);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_H_
