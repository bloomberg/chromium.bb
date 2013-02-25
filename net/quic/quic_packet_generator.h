// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Responsible for generating packets on behalf of a QuicConnection.
// Packets are serialized just-in-time.  Control frame are queued.
// Ack and Feedback frames will requested from the Connection
// just-in-time.  When a packet needs to be sent, the Generator
// wills serialized a packet pass it to  QuicConnection::SendOrQueuePacket()
//
// The Generator's mode of operation is controlled by two conditions:
//
// 1) Is the Delegate writable?
//
// If the Delegate is not writable, then no operations will cause
// a packet to be serialized.  In particular:
// * SetShouldSendAck will simply record that an ack is to be send.
// * AddControlFram will enqueue the control frame.
// * ConsumeData will do nothing.
//
// If the Delegate is writable, then the behavior depends on the second
// condition:
//
// 2) Is the Generator in batch mode?
//
// If the Generator is NOT in batch mode, then each call to a write
// operation will serialize one or more packets.  The contents will
// include any previous queued frames.  If an ack should be sent
// but has not been sent, then the Delegate will be asked to create
// an Ack frame which will then be included in the packet.  When
// the write call completes, the current packet will be serialized
// and sent to the Delegate, even if it is not full.
//
// If the Generator is in batch mode, then each write operation will
// add data to the "current" packet.  When the current packet becomes
// full, it will be serialized and sent to the packet.  When batch
// mode is ended via |FinishBatchOperations|, the current packet
// will be serialzied, even if it is not full.
//
// FEC behavior also depends on batch mode.  In batch mode, FEC packets
// will be sent after |max_packets_per_group| have been sent, as well
// as after batch operations are complete.  When not in batch mode,
// an FEC packet will be sent after each write call completes.
//
// TODO(rch): This behavior should probably be tuned.  When not in batch
// mode we, should probably set a timer so that several independent
// operations can be grouped into the same FEC group.
//
// When an FEC packet is generate, it will be send to the Delegate,
// even if the Delegate has become unwritable after handling the
// data packet immediately proceeding the FEC packet.

#ifndef NET_QUIC_QUIC_PACKET_GENERATOR_H_
#define NET_QUIC_QUIC_PACKET_GENERATOR_H_

#include "net/quic/quic_packet_creator.h"

namespace net {

class NET_EXPORT_PRIVATE QuicPacketGenerator {
 public:
  class NET_EXPORT_PRIVATE DelegateInterface {
   public:
    virtual ~DelegateInterface() {}
    virtual bool CanWrite(bool is_retransmission) = 0;
    virtual QuicAckFrame* CreateAckFrame() = 0;
    virtual QuicCongestionFeedbackFrame* CreateFeedbackFrame() = 0;
    // Takes ownership of |packet.packet| and |packet.retransmittable_frames|.
    virtual bool OnSerializedPacket(const SerializedPacket& packet) = 0;
  };

  QuicPacketGenerator(DelegateInterface* delegate,
                      QuicPacketCreator* creator);

  virtual ~QuicPacketGenerator();

  void SetShouldSendAck(bool also_send_feedback);
  void AddControlFrame(const QuicFrame& frame);
  QuicConsumedData ConsumeData(QuicStreamId id,
                               base::StringPiece data,
                               QuicStreamOffset offset,
                               bool fin);

  // Disables flushing.
  void StartBatchOperations();
  // Enables flushing and flushes queued data.
  void FinishBatchOperations();

  bool HasQueuedData() const;

 private:
  // Must be called when the connection is writable
  // and not blocked by the congestion manager.
  void SendQueuedData();

  bool HasPendingData() const;
  bool AddNextPendingFrame();
  void SerializeAndSendPacket();

  DelegateInterface* delegate_;

  QuicPacketCreator* packet_creator_;
  QuicFrames queued_control_frames_;
  bool should_flush_;
  bool should_send_ack_;
  scoped_ptr<QuicAckFrame> pending_ack_frame_;
  scoped_ptr<QuicCongestionFeedbackFrame> pending_feedback_frame_;
  bool should_send_feedback_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PACKET_GENERATOR_H_
