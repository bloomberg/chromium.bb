// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Fix rate send side congestion control, used for testing.

#ifndef NET_QUIC_CONGESTION_CONTROL_FIX_RATE_SENDER_H_
#define NET_QUIC_CONGESTION_CONTROL_FIX_RATE_SENDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_time.h"
#include "net/quic/congestion_control/leaky_bucket.h"
#include "net/quic/congestion_control/paced_sender.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"

namespace net {

namespace test {
class FixRateSenderPeer;
}  // namespace test

class NET_EXPORT_PRIVATE FixRateSender : public SendAlgorithmInterface {
 public:
  explicit FixRateSender(const QuicClock* clock);

  // Start implementation of SendAlgorithmInterface.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& feedback,
      const SentPacketsMap& sent_packets) OVERRIDE;
  virtual void OnIncomingAck(QuicPacketSequenceNumber acked_sequence_number,
                             QuicByteCount acked_bytes,
                             QuicTime::Delta rtt) OVERRIDE;
  virtual void OnIncomingLoss(int number_of_lost_packets) OVERRIDE;
  virtual void SentPacket(QuicPacketSequenceNumber equence_number,
                          QuicByteCount bytes,
                          bool is_retransmission) OVERRIDE;
  virtual QuicTime::Delta TimeUntilSend(bool is_retransmission) OVERRIDE;
  virtual QuicBandwidth BandwidthEstimate() OVERRIDE;
  // End implementation of SendAlgorithmInterface.

 private:
  friend class test::FixRateSenderPeer;

  QuicByteCount AvailableCongestionWindow();
  QuicByteCount CongestionWindow();

  QuicBandwidth bitrate_;
  LeakyBucket fix_rate_leaky_bucket_;
  PacedSender paced_sender_;
  QuicByteCount data_in_flight_;

  DISALLOW_COPY_AND_ASSIGN(FixRateSender);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_FIX_RATE_SENDER_H_
