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
#include <set>
#include <vector>

#include "base/hash_tables.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/congestion_control/quic_congestion_manager.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
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
  typedef std::set<QuicPacketSequenceNumber> AckedPackets;

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

  // Called when the connection is closed either locally by the framer, or
  // remotely by the peer.
  virtual void ConnectionClose(QuicErrorCode error, bool from_peer) = 0;

  // Called when packets are acked by the peer.
  virtual void OnAck(AckedPackets acked_packets) = 0;

  // Called when a blocked socket becomes writable.  If all pending bytes for
  // this visitor are consumed by the connection successfully this should
  // return true, otherwise it should return false.
  virtual bool OnCanWrite() = 0;
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

class NET_EXPORT_PRIVATE QuicConnection : public QuicFramerVisitorInterface {
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
                             QuicErrorCode error,
                             QuicStreamOffset offset);
  // Sends a connection close frame to the peer, and closes the connection by
  // calling CloseConnection(notifying the visitor as it does so).
  virtual void SendConnectionClose(QuicErrorCode error);
  // Notifies the visitor of the close and marks the connection as disconnected.
  void CloseConnection(QuicErrorCode error, bool from_peer);

  // Processes an incoming UDP packet (consisting of a QuicEncryptedPacket) from
  // the peer.  If processing this packet permits a packet to be revived from
  // its FEC group that packet will be revived and processed.
  virtual void ProcessUdpPacket(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet);

  // Called when the underlying connection becomes writable to allow
  // queued writes to happen.  Returns false if the socket has become blocked.
  virtual bool OnCanWrite();

  // From QuicFramerVisitorInterface
  virtual void OnError(QuicFramer* framer) OVERRIDE;
  virtual void OnPacket(const IPEndPoint& self_address,
                        const IPEndPoint& peer_address) OVERRIDE;
  virtual void OnRevivedPacket() OVERRIDE;
  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnFecProtectedPayload(base::StringPiece payload) OVERRIDE;
  virtual void OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE;
  virtual void OnAckFrame(const QuicAckFrame& frame) OVERRIDE;
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE;
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE;
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) OVERRIDE;
  virtual void OnFecData(const QuicFecData& fec) OVERRIDE;
  virtual void OnPacketComplete() OVERRIDE;

  // Accessors
  void set_visitor(QuicConnectionVisitorInterface* visitor) {
    visitor_ = visitor;
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
  void MaybeRetransmitPacket(QuicPacketSequenceNumber sequence_number);

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
  // Send a packet to the peer.  If should_retransmit is true, this packet
  // contains data, and contents will be retransmitted with a new sequence
  // number if we don't get an ack.  If force is true, then the packet will
  // be sent immediately and the send scheduler will not be consulted.  If
  // is_retransmission is true, this packet is being retransmitted with a new
  // sequence number.  Always takes ownership of packet.
  // TODO(wtc): none of the callers check the return value.
  virtual bool SendPacket(QuicPacketSequenceNumber number,
                          QuicPacket* packet,
                          bool should_retransmit,
                          bool force,
                          bool is_retransmission);

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

  // Utility which sets SetLeastUnacked to least_unacked, and updates the list
  // of non-retransmitting packets accordingly.
  void SetLeastUnacked(QuicPacketSequenceNumber least_unacked);

  // Helper to update least unacked.  If acked_sequence_number was not the least
  // unacked packet, this is a no-op.  If it was the least unacked packet,
  // this finds the new least unacked packet and updates the outgoing ack frame.
  void UpdateLeastUnacked(QuicPacketSequenceNumber acked_sequence_number);

  QuicConnectionHelperInterface* helper() { return helper_; }

 private:
  friend class test::QuicConnectionPeer;
  // Packets which have not been written to the wire.
  struct QueuedPacket {
    QueuedPacket(QuicPacketSequenceNumber sequence_number,
                 QuicPacket* packet,
                 bool should_retransmit,
                 bool is_retransmission)
        : sequence_number(sequence_number),
          packet(packet),
          should_retransmit(should_retransmit),
          is_retransmission(is_retransmission) {
    }

    QuicPacketSequenceNumber sequence_number;
    QuicPacket* packet;
    bool should_retransmit;
    bool is_retransmission;
  };

  struct UnackedPacket {
    explicit UnackedPacket(QuicFrames unacked_frames);
    UnackedPacket(QuicFrames unacked_frames, std::string data);
    ~UnackedPacket();

    QuicFrames frames;
    uint8 number_nacks;
    // Data referenced by the StringPiece of a QuicStreamFrame.
    std::string data;
  };

  typedef std::list<QueuedPacket> QueuedPacketList;
  typedef base::hash_map<QuicPacketSequenceNumber,
      UnackedPacket*> UnackedPacketMap;
  typedef std::map<QuicFecGroupNumber, QuicFecGroup*> FecGroupMap;
  typedef std::list<std::pair<QuicPacketSequenceNumber, QuicTime> >
      RetransmissionTimeouts;

  // The amount of time we wait before retransmitting a packet.
  static const QuicTime::Delta DefaultRetransmissionTime() {
    return QuicTime::Delta::FromMilliseconds(500);
  }

  static void DeleteUnackedPacket(UnackedPacket* unacked);
  static bool ShouldRetransmit(const QuicFrame& frame);

  // Checks if a packet can be written now, and sets the timer if necessary.
  bool CanWrite(bool is_retransmission);

  // Writes as much queued data as possible.  The connection must not be
  // blocked when this is called.
  bool WriteData();

  // If a packet can be revived from the current FEC group, then
  // revive and process the packet.
  void MaybeProcessRevivedPacket();

  void MaybeSendAckInResponseToPacket();

  // Get the FEC group associate with the last processed packet.
  QuicFecGroup* GetFecGroup();

  // Fills the ack frame with the appropriate latched information.
  void FillAckFrame(QuicAckFrame* ack);

  // Closes any FEC groups protecting packets before |sequence_number|.
  void CloseFecGroupsBefore(QuicPacketSequenceNumber sequence_number);

  QuicConnectionHelperInterface* helper_;
  QuicFramer framer_;
  const QuicClock* clock_;
  QuicRandom* random_generator_;

  const QuicGuid guid_;
  IPEndPoint self_address_;
  IPEndPoint peer_address_;

  bool last_packet_revived_;  // True if the last packet was revived from FEC.
  size_t last_size_;  // Size of the last received packet.
  QuicPacketHeader last_header_;
  std::vector<QuicStreamFrame> last_stream_frames_;

  bool should_send_ack_;
  bool should_send_congestion_feedback_;
  QuicAckFrame outgoing_ack_;
  QuicCongestionFeedbackFrame outgoing_congestion_feedback_;

  // Track some client state so we can do less bookkeeping
  QuicPacketSequenceNumber largest_seen_packet_with_ack_;
  QuicPacketSequenceNumber peer_largest_observed_packet_;
  QuicPacketSequenceNumber peer_least_packet_awaiting_ack_;

  // When new packets are created which may be retransmitted, they are added
  // to this map, which contains owning pointers to the contained frames.
  UnackedPacketMap unacked_packets_;

  // List of packets that we might need to retransmission, and the time at
  // which we should retransmission them.  This is currently a FIFO queue which
  // means we will never fire an RTO for packet 2 if we are waiting to fire
  // the RTO for packet 1.  This logic is likely suboptimal but it will
  // change when it moves to the SendScheduler so it is fine for now.
  RetransmissionTimeouts retransmission_timeouts_;

  // True while OnRetransmissionTimeout is running to prevent
  // SetRetransmissionAlarm from being called erroneously.
  bool handling_retransmission_timeout_;

  // When packets could not be sent because the socket was not writable,
  // they are added to this list.  All corresponding frames are in
  // unacked_packets_ if they are to be retransmitted.
  QueuedPacketList queued_packets_;

  // Pending control frames, besides the ack and congestion control frames.
  QuicFrames queued_control_frames_;

  // True when the socket becomes unwritable.
  bool write_blocked_;

  FecGroupMap group_map_;
  QuicPacketHeader revived_header_;
  scoped_array<char> revived_payload_;

  QuicConnectionVisitorInterface* visitor_;
  QuicPacketCreator packet_creator_;

  // Network idle time before we kill of this connection.
  const QuicTime::Delta timeout_;
  // The time that we got or tried to send a packet for this connection.
  QuicTime time_of_last_packet_;

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
