// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TCP cubic send side congestion algorithm, emulates the behaviour of
// TCP cubic.

#ifndef NET_QUIC_CONGESTION_CONTROL_TCP_CUBIC_SENDER_H_
#define NET_QUIC_CONGESTION_CONTROL_TCP_CUBIC_SENDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/cubic.h"
#include "net/quic/congestion_control/hybrid_slow_start.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

namespace test {
class TcpCubicSenderPeer;
}  // namespace test

class NET_EXPORT_PRIVATE TcpCubicSender : public SendAlgorithmInterface {
 public:
  // Reno option and max_tcp_congestion_window are provided for testing.
  TcpCubicSender(const QuicClock* clock,
                 bool reno,
                 QuicTcpCongestionWindow max_tcp_congestion_window);
  virtual ~TcpCubicSender();

  // Start implementation of SendAlgorithmInterface.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& feedback,
      QuicTime feedback_receive_time,
      const SentPacketsMap& sent_packets) OVERRIDE;
  virtual void OnIncomingAck(QuicPacketSequenceNumber acked_sequence_number,
                             QuicByteCount acked_bytes,
                             QuicTime::Delta rtt) OVERRIDE;
  virtual void OnIncomingLoss(QuicTime ack_receive_time) OVERRIDE;
  virtual void SentPacket(QuicTime sent_time,
                          QuicPacketSequenceNumber sequence_number,
                          QuicByteCount bytes,
                          Retransmission is_retransmission) OVERRIDE;
  virtual void AbandoningPacket(QuicPacketSequenceNumber sequence_number,
                                QuicByteCount abandoned_bytes) OVERRIDE;
  virtual QuicTime::Delta TimeUntilSend(
      QuicTime now,
      Retransmission is_retransmission,
      HasRetransmittableData has_retransmittable_data,
      IsHandshake handshake) OVERRIDE;
  virtual QuicBandwidth BandwidthEstimate() OVERRIDE;
  virtual QuicTime::Delta SmoothedRtt() OVERRIDE;
  virtual QuicTime::Delta RetransmissionDelay() OVERRIDE;
  // End implementation of SendAlgorithmInterface.

 private:
  friend class test::TcpCubicSenderPeer;

  QuicByteCount AvailableCongestionWindow();
  QuicByteCount CongestionWindow();
  void Reset();
  void AckAccounting(QuicTime::Delta rtt);
  void CongestionAvoidance(QuicPacketSequenceNumber ack);
  bool IsCwndLimited() const;
  void OnTimeOut();

  HybridSlowStart hybrid_slow_start_;
  Cubic cubic_;

  // Reno provided for testing.
  const bool reno_;

  // ACK counter for the Reno implementation.
  int64 congestion_window_count_;

  // Receiver side advertised window.
  QuicByteCount receiver_congestion_window_;

  // Receiver side advertised packet loss.
  int last_received_accumulated_number_of_lost_packets_;

  // Bytes in flight, aka bytes on the wire.
  QuicByteCount bytes_in_flight_;

  // We need to keep track of the end sequence number of each RTT "burst".
  bool update_end_sequence_number_;
  QuicPacketSequenceNumber end_sequence_number_;

  // Congestion window in packets.
  QuicTcpCongestionWindow congestion_window_;

  // Slow start congestion window in packets.
  QuicTcpCongestionWindow slowstart_threshold_;

  // Maximum number of outstanding packets for tcp.
  QuicTcpCongestionWindow max_tcp_congestion_window_;

  // Min RTT during this session.
  QuicTime::Delta delay_min_;

  // Smoothed RTT during this session.
  QuicTime::Delta smoothed_rtt_;

  // Mean RTT deviation during this session.
  // Approximation of standard deviation, the error is roughly 1.25 times
  // larger than the standard deviation, for a normally distributed signal.
  QuicTime::Delta mean_deviation_;

  DISALLOW_COPY_AND_ASSIGN(TcpCubicSender);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_TCP_CUBIC_SENDER_H_
