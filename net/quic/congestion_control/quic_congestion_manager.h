// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the interface from the QuicConnection into the QUIC
// congestion control code.  It wraps the SendAlgorithmInterface and
// ReceiveAlgorithmInterface and provides a single interface
// for consumers.

#ifndef NET_QUIC_CONGESTION_CONTROL_QUIC_CONGESTION_MANAGER_H_
#define NET_QUIC_CONGESTION_CONTROL_QUIC_CONGESTION_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace test {
class QuicConnectionPeer;
class QuicCongestionManagerPeer;
}  // namespace test

class QuicClock;
class ReceiveAlgorithmInterface;

class NET_EXPORT_PRIVATE QuicCongestionManager {
 public:
  QuicCongestionManager(const QuicClock* clock,
                        CongestionFeedbackType congestion_type);
  virtual ~QuicCongestionManager();

  virtual void SetFromConfig(const QuicConfig& config, bool is_server);

  // Called when we have received an ack frame from peer.
  virtual void OnIncomingAckFrame(const QuicAckFrame& frame,
                                  QuicTime ack_receive_time);

  // Called when a congestion feedback frame is received from peer.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame,
      QuicTime feedback_receive_time);

  // Called when we have sent bytes to the peer.  This informs the manager both
  // the number of bytes sent and if they were retransmitted.
  virtual void OnPacketSent(QuicPacketSequenceNumber sequence_number,
                            QuicTime sent_time,
                            QuicByteCount bytes,
                            TransmissionType transmission_type,
                            HasRetransmittableData has_retransmittable_data);

  // Called when a packet is timed out.
  virtual void OnPacketAbandoned(QuicPacketSequenceNumber sequence_number);

  // Calculate the time until we can send the next packet to the wire.
  // Note 1: When kUnknownWaitTime is returned, there is no need to poll
  // TimeUntilSend again until we receive an OnIncomingAckFrame event.
  // Note 2: Send algorithms may or may not use |retransmit| in their
  // calculations.
  virtual QuicTime::Delta TimeUntilSend(QuicTime now,
                                        TransmissionType transmission_type,
                                        HasRetransmittableData retransmittable,
                                        IsHandshake handshake);

  // Should be called before sending an ACK packet, to decide if we need
  // to attach a QuicCongestionFeedbackFrame block.
  // Returns false if no QuicCongestionFeedbackFrame block is needed.
  // Otherwise fills in feedback and returns true.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback);

  // Should be called for each incoming packet.
  // bytes: the packet size in bytes including Quic Headers.
  // sequence_number: the unique sequence number from the QUIC packet header.
  // timestamp: the arrival time of the packet.
  // revived: true if the packet was lost and then recovered with help of a
  // FEC packet.
  virtual void RecordIncomingPacket(QuicByteCount bytes,
                                    QuicPacketSequenceNumber sequence_number,
                                    QuicTime timestamp,
                                    bool revived);

  const QuicTime::Delta DefaultRetransmissionTime();

  // Returns amount of time for delayed ack timer.
  const QuicTime::Delta DelayedAckTime();

  const QuicTime::Delta GetRetransmissionDelay(
      size_t unacked_packets_count,
      size_t number_retransmissions);

  // Returns the estimated smoothed RTT calculated by the congestion algorithm.
  const QuicTime::Delta SmoothedRtt();

  // Returns the estimated bandwidth calculated by the congestion algorithm.
  QuicBandwidth BandwidthEstimate();

  // Returns the size of the current congestion window in bytes.  Note, this is
  // not the *available* window.  Some send algorithms may not use a congestion
  // window and will return 0.
  QuicByteCount GetCongestionWindow() const;

  // Sets the value of the current congestion window to |window|.
  void SetCongestionWindow(QuicByteCount window);

 private:
  friend class test::QuicConnectionPeer;
  friend class test::QuicCongestionManagerPeer;
  typedef std::map<QuicPacketSequenceNumber, size_t> PendingPacketsMap;

  // Get the current(last) rtt. Infinite is returned if invalid.
  const QuicTime::Delta rtt();

  void CleanupPacketHistory();

  const QuicClock* clock_;
  scoped_ptr<ReceiveAlgorithmInterface> receive_algorithm_;
  scoped_ptr<SendAlgorithmInterface> send_algorithm_;
  SendAlgorithmInterface::SentPacketsMap packet_history_map_;
  PendingPacketsMap pending_packets_;
  QuicPacketSequenceNumber largest_missing_;
  QuicTime::Delta current_rtt_;

  DISALLOW_COPY_AND_ASSIGN(QuicCongestionManager);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_QUIC_CONGESTION_MANAGER_H_
