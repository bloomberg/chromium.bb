// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A send algorithm which adds pacing on top of an another send algorithm.
// It uses the underlying sender's bandwidth estimate to determine the
// pacing rate to be used.  It also takes into consideration the expected
// resolution of the underlying alarm mechanism to ensure that alarms are
// not set too aggressively, and to smooth out variations.

#ifndef NET_QUIC_CONGESTION_CONTROL_PACING_SENDER_H_
#define NET_QUIC_CONGESTION_CONTROL_PACING_SENDER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class NET_EXPORT_PRIVATE PacingSender : public SendAlgorithmInterface {
 public:
  PacingSender(SendAlgorithmInterface* sender,
               QuicTime::Delta alarm_granularity);
  virtual ~PacingSender();

  // SendAlgorithmInterface methods.
  virtual void SetFromConfig(const QuicConfig& config, bool is_server) OVERRIDE;
  virtual void SetMaxPacketSize(QuicByteCount max_packet_size) OVERRIDE;
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& feedback,
      QuicTime feedback_receive_time,
      const SendAlgorithmInterface::SentPacketsMap& sent_packets) OVERRIDE;
  virtual void OnPacketAcked(QuicPacketSequenceNumber acked_sequence_number,
                             QuicByteCount acked_bytes,
                             QuicTime::Delta rtt) OVERRIDE;
  virtual void OnPacketLost(QuicPacketSequenceNumber sequence_number,
                            QuicTime ack_receive_time) OVERRIDE;
  virtual bool OnPacketSent(QuicTime sent_time,
                            QuicPacketSequenceNumber sequence_number,
                            QuicByteCount bytes,
                            TransmissionType transmission_type,
                            HasRetransmittableData is_retransmittable) OVERRIDE;
  virtual void OnRetransmissionTimeout() OVERRIDE;
  virtual void OnPacketAbandoned(QuicPacketSequenceNumber sequence_number,
                                 QuicByteCount abandoned_bytes) OVERRIDE;
  virtual QuicTime::Delta TimeUntilSend(
      QuicTime now,
      TransmissionType transmission_type,
      HasRetransmittableData has_retransmittable_data,
      IsHandshake handshake) OVERRIDE;
  virtual QuicBandwidth BandwidthEstimate() const OVERRIDE;
  virtual QuicTime::Delta SmoothedRtt() const OVERRIDE;
  virtual QuicTime::Delta RetransmissionDelay() const OVERRIDE;
  virtual QuicByteCount GetCongestionWindow() const OVERRIDE;

 private:
  QuicTime::Delta GetTransferTime(QuicByteCount bytes);

  scoped_ptr<SendAlgorithmInterface> sender_;  // Underlying sender.
  QuicTime::Delta alarm_granularity_;
  QuicTime next_packet_send_time_;  // When can the next packet be sent.
  bool was_last_send_delayed_;  // True when the last send was delayed.
  QuicByteCount max_segment_size_;

  DISALLOW_COPY_AND_ASSIGN(PacingSender);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_PACING_SENDER_H_
