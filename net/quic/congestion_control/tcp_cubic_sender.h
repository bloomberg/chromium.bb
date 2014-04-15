// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TCP cubic send side congestion algorithm, emulates the behavior of
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
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

// Default maximum packet size used in Linux TCP implementations.
const QuicByteCount kDefaultTCPMSS = 1460;

class RttStats;

namespace test {
class TcpCubicSenderPeer;
}  // namespace test

class NET_EXPORT_PRIVATE TcpCubicSender : public SendAlgorithmInterface {
 public:
  // Reno option and max_tcp_congestion_window are provided for testing.
  TcpCubicSender(const QuicClock* clock,
                 const RttStats* rtt_stats,
                 bool reno,
                 QuicTcpCongestionWindow max_tcp_congestion_window,
                 QuicConnectionStats* stats);
  virtual ~TcpCubicSender();

  bool InSlowStart() const;

  // Start implementation of SendAlgorithmInterface.
  virtual void SetFromConfig(const QuicConfig& config, bool is_server) OVERRIDE;
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& feedback,
      QuicTime feedback_receive_time) OVERRIDE;
  virtual void OnPacketAcked(QuicPacketSequenceNumber acked_sequence_number,
                             QuicByteCount acked_bytes) OVERRIDE;
  virtual void OnPacketLost(QuicPacketSequenceNumber largest_loss,
                            QuicTime ack_receive_time) OVERRIDE;
  virtual bool OnPacketSent(QuicTime sent_time,
                            QuicPacketSequenceNumber sequence_number,
                            QuicByteCount bytes,
                            HasRetransmittableData is_retransmittable) OVERRIDE;
  virtual void OnRetransmissionTimeout(bool packets_retransmitted) OVERRIDE;
  virtual void OnPacketAbandoned(QuicPacketSequenceNumber sequence_number,
                                 QuicByteCount abandoned_bytes) OVERRIDE;
  virtual QuicTime::Delta TimeUntilSend(
      QuicTime now,
      HasRetransmittableData has_retransmittable_data) OVERRIDE;
  virtual QuicBandwidth BandwidthEstimate() const OVERRIDE;
  virtual void UpdateRtt(QuicTime::Delta rtt_sample) OVERRIDE;
  virtual QuicTime::Delta RetransmissionDelay() const OVERRIDE;
  virtual QuicByteCount GetCongestionWindow() const OVERRIDE;
  // End implementation of SendAlgorithmInterface.

 private:
  friend class test::TcpCubicSenderPeer;

  QuicByteCount AvailableSendWindow();
  QuicByteCount SendWindow();
  void MaybeIncreaseCwnd(QuicPacketSequenceNumber acked_sequence_number);
  bool IsCwndLimited() const;
  bool InRecovery() const;
  // Methods for isolating PRR from the rest of TCP Cubic.
  void PrrOnPacketLost();
  void PrrOnPacketAcked(QuicByteCount acked_bytes);
  QuicTime::Delta PrrTimeUntilSend();


  HybridSlowStart hybrid_slow_start_;
  Cubic cubic_;
  const RttStats* rtt_stats_;
  QuicConnectionStats* stats_;

  // Reno provided for testing.
  const bool reno_;

  // ACK counter for the Reno implementation.
  int64 congestion_window_count_;

  // Receiver side advertised window.
  QuicByteCount receive_window_;

  // Bytes in flight, aka bytes on the wire.
  QuicByteCount bytes_in_flight_;

  // Bytes sent and acked since the last loss event.  Used for PRR.
  QuicByteCount prr_out_;
  QuicByteCount prr_delivered_;
  size_t ack_count_since_loss_;

  // The congestion window before the last loss event.
  QuicByteCount bytes_in_flight_before_loss_;

  // Track the largest packet that has been sent.
  QuicPacketSequenceNumber largest_sent_sequence_number_;

  // Track the largest packet that has been acked.
  QuicPacketSequenceNumber largest_acked_sequence_number_;

  // Track the largest sequence number outstanding when a CWND cutback occurs.
  QuicPacketSequenceNumber largest_sent_at_last_cutback_;

  // Congestion window in packets.
  QuicTcpCongestionWindow congestion_window_;

  // Slow start congestion window in packets, aka ssthresh.
  QuicTcpCongestionWindow slowstart_threshold_;

  // Whether the last loss event caused us to exit slowstart.
  // Used for stats collection of slowstart_packets_lost
  bool last_cutback_exited_slowstart_;

  // Maximum number of outstanding packets for tcp.
  QuicTcpCongestionWindow max_tcp_congestion_window_;

  DISALLOW_COPY_AND_ASSIGN(TcpCubicSender);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_TCP_CUBIC_SENDER_H_
