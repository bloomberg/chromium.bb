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

NET_EXPORT_PRIVATE extern bool FLAGS_limit_rto_increase_for_tests;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_pacing;

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

  virtual void SetMaxPacketSize(QuicByteCount max_packet_size);

  // Called when we have received an ack frame from peer.
  // Returns a set containing all the sequence numbers to be nack retransmitted
  // as a result of the ack.
  virtual SequenceNumberSet OnIncomingAckFrame(const QuicAckFrame& frame,
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

  // Called when the retransmission timer expires.
  virtual void OnRetransmissionTimeout();

  // Called when the least unacked sequence number increases, indicating the
  // consecutive rto count should be reset to 0.
  virtual void OnLeastUnackedIncreased();

  // Called when a packet is timed out, such as an RTO.  Removes the bytes from
  // the congestion manager, but does not change the congestion window size.
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

  const QuicTime::Delta DefaultRetransmissionTime();

  // Returns amount of time for delayed ack timer.
  const QuicTime::Delta DelayedAckTime();

  // Returns the current RTO delay.
  const QuicTime::Delta GetRetransmissionDelay() const;

  // Returns the estimated smoothed RTT calculated by the congestion algorithm.
  const QuicTime::Delta SmoothedRtt() const;

  // Returns the estimated bandwidth calculated by the congestion algorithm.
  QuicBandwidth BandwidthEstimate() const;

  // Returns the size of the current congestion window in bytes.  Note, this is
  // not the *available* window.  Some send algorithms may not use a congestion
  // window and will return 0.
  QuicByteCount GetCongestionWindow() const;

  // Sets the value of the current congestion window to |window|.
  void SetCongestionWindow(QuicByteCount window);

  // Enables pacing if it has not already been enabled, and if
  // FLAGS_enable_quic_pacing is set.
  void MaybeEnablePacing();

  bool using_pacing() const { return using_pacing_; }

 private:
  friend class test::QuicConnectionPeer;
  friend class test::QuicCongestionManagerPeer;

  void CleanupPacketHistory();

  const QuicClock* clock_;
  scoped_ptr<SendAlgorithmInterface> send_algorithm_;
  // Tracks the send time, size, and nack count of sent packets.  Packets are
  // removed after 5 seconds and they've been removed from pending_packets_.
  SendAlgorithmInterface::SentPacketsMap packet_history_map_;
  // Packets that are outstanding and have not been abandoned or lost.
  SequenceNumberSet pending_packets_;
  QuicPacketSequenceNumber largest_missing_;
  QuicTime::Delta rtt_sample_;  // RTT estimate from the most recent ACK.
  // Number of times the RTO timer has fired in a row without receiving an ack.
  size_t consecutive_rto_count_;
  bool using_pacing_;

  DISALLOW_COPY_AND_ASSIGN(QuicCongestionManager);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_QUIC_CONGESTION_MANAGER_H_
