// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the interface from the QuicConnection into the QUIC
// congestion control code.  It wraps the QuicSendScheduler and
// QuicReceiptMetricsCollector and provides a single interface
// for consumers.

#ifndef NET_QUIC_CONGESTION_CONTROL_QUIC_CONGESTION_MANAGER_H_
#define NET_QUIC_CONGESTION_CONTROL_QUIC_CONGESTION_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace test {
class QuicConnectionPeer;
}  // namespace test

class QuicClock;
class QuicReceiptMetricsCollector;
class QuicSendScheduler;

class QuicCongestionManager {
 public:
  QuicCongestionManager(const QuicClock* clock,
                        CongestionFeedbackType congestion_type);
  virtual ~QuicCongestionManager();

  // Called when we have received an ack frame from peer.
  virtual void OnIncomingAckFrame(const QuicAckFrame& frame);

  // Called when a congestion feedback frame is received from peer.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame);

  // Called when we have sent bytes to the peer.  This informs the manager both
  // the number of bytes sent and if they were retransmitted.
  virtual void SentPacket(QuicPacketSequenceNumber sequence_number,
                          size_t bytes,
                          bool is_retransmission);

  // Calculate the time until we can send the next packet to the wire.
  // Note 1: When kUnknownWaitTime is returned, there is no need to poll
  // TimeUntilSend again until we receive an OnIncomingAckFrame event.
  // Note 2: Send algorithms may or may not use |retransmit| in their
  // calculations.
  virtual QuicTime::Delta TimeUntilSend(bool is_retransmission);

  // Should be called before sending an ACK packet, to decide if we need
  // to attach a QuicCongestionFeedbackFrame block.
  // Returns false if no QuicCongestionFeedbackFrame block is needed.
  // Otherwise fills in feedback and returns true.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback);

  // Should be called for each incoming packet.
  // bytes: the packet size in bytes including IP headers.
  // sequence_number: the unique sequence number from the QUIC packet header.
  // timestamp: the arrival time of the packet.
  // revived: true if the packet was lost and then recovered with help of a
  // FEC packet.
  virtual void RecordIncomingPacket(size_t bytes,
                                    QuicPacketSequenceNumber sequence_number,
                                    QuicTime timestamp,
                                    bool revived);

 private:
  friend class test::QuicConnectionPeer;

  scoped_ptr<QuicReceiptMetricsCollector> collector_;
  scoped_ptr<QuicSendScheduler> scheduler_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_QUIC_CONGESTION_MANAGER_H_
