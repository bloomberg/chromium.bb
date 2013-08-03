// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Responsible for generating packets on behalf of a QuicConnection.
// Packets are serialized just-in-time.  Control frames are queued.
// Ack and Feedback frames will be requested from the Connection
// just-in-time.  When a packet needs to be sent, the Generator
// will serialize a packet and pass it to QuicConnection::SendOrQueuePacket()
//
// The Generator's mode of operation is controlled by two conditions:
//
// 1) Is the Delegate writable?
//
// If the Delegate is not writable, then no operations will cause
// a packet to be serialized.  In particular:
// * SetShouldSendAck will simply record that an ack is to be sent.
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
// mode, we should probably set a timer so that several independent
// operations can be grouped into the same FEC group.
//
// When an FEC packet is generated, it will be send to the Delegate,
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
    virtual bool CanWrite(Retransmission retransmission,
                          HasRetransmittableData retransmittable,
                          IsHandshake handshake) = 0;
    virtual QuicAckFrame* CreateAckFrame() = 0;
    virtual QuicCongestionFeedbackFrame* CreateFeedbackFrame() = 0;
    // Takes ownership of |packet.packet| and |packet.retransmittable_frames|.
    virtual bool OnSerializedPacket(const SerializedPacket& packet) = 0;
  };

  // Interface which gets callbacks from the QuicPacketGenerator at interesting
  // points.  Implementations must not mutate the state of the generator
  // as a result of these callbacks.
  class NET_EXPORT_PRIVATE DebugDelegateInterface {
   public:
    virtual ~DebugDelegateInterface() {}

    // Called when a frame has been added to the current packet.
    virtual void OnFrameAddedToPacket(const QuicFrame& frame) = 0;
  };

  QuicPacketGenerator(DelegateInterface* delegate,
                      DebugDelegateInterface* debug_delegate,
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

  bool HasQueuedFrames() const;

  void set_debug_delegate(DebugDelegateInterface* debug_delegate) {
    debug_delegate_ = debug_delegate;
  }

 private:
  void SendQueuedFrames();

  // Test to see if we have pending ack, feedback, or control frames.
  bool HasPendingFrames() const;
  // Test to see if the addition of a pending frame (which might be
  // retransmittable) would still allow the resulting packet to be sent now.
  bool CanSendWithNextPendingFrameAddition() const;
  // Add exactly one pending frame, preferring ack over feedback over control
  // frames.
  bool AddNextPendingFrame();

  bool AddFrame(const QuicFrame& frame);
  void SerializeAndSendPacket();

  DelegateInterface* delegate_;
  DebugDelegateInterface* debug_delegate_;

  QuicPacketCreator* packet_creator_;
  QuicFrames queued_control_frames_;
  bool should_flush_;
  // Flags to indicate the need for just-in-time construction of a frame.
  bool should_send_ack_;
  bool should_send_feedback_;
  // If we put a non-retransmittable frame (namley ack or feedback frame) in
  // this packet, then we have to hold a reference to it until we flush (and
  // serialize it). Retransmittable frames are referenced elsewhere so that they
  // can later be (optionally) retransmitted.
  scoped_ptr<QuicAckFrame> pending_ack_frame_;
  scoped_ptr<QuicCongestionFeedbackFrame> pending_feedback_frame_;

  DISALLOW_COPY_AND_ASSIGN(QuicPacketGenerator);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PACKET_GENERATOR_H_
