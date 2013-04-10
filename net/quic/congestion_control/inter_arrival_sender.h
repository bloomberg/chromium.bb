// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_SENDER_H_
#define NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_SENDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/channel_estimator.h"
#include "net/quic/congestion_control/inter_arrival_bitrate_ramp_up.h"
#include "net/quic/congestion_control/inter_arrival_overuse_detector.h"
#include "net/quic/congestion_control/inter_arrival_probe.h"
#include "net/quic/congestion_control/inter_arrival_state_machine.h"
#include "net/quic/congestion_control/paced_sender.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class NET_EXPORT_PRIVATE InterArrivalSender : public SendAlgorithmInterface {
 public:
  explicit InterArrivalSender(const QuicClock* clock);
  virtual ~InterArrivalSender();

  // Start implementation of SendAlgorithmInterface.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& feedback,
      QuicTime feedback_receive_time,
      QuicBandwidth sent_bandwidth,
      const SentPacketsMap& sent_packets) OVERRIDE;

  virtual void OnIncomingAck(QuicPacketSequenceNumber acked_sequence_number,
                             QuicByteCount acked_bytes,
                             QuicTime::Delta rtt) OVERRIDE;

  virtual void OnIncomingLoss(QuicTime ack_receive_time) OVERRIDE;

  virtual void SentPacket(QuicTime sent_time,
                          QuicPacketSequenceNumber sequence_number,
                          QuicByteCount bytes,
                          Retransmission is_retransmit) OVERRIDE;

  virtual void AbandoningPacket(QuicPacketSequenceNumber sequence_number,
                                QuicByteCount abandoned_bytes) OVERRIDE;

  virtual QuicTime::Delta TimeUntilSend(
      QuicTime now,
      Retransmission is_retransmission,
      HasRetransmittableData has_retransmittable_data) OVERRIDE;

  virtual QuicBandwidth BandwidthEstimate() OVERRIDE;
  virtual QuicTime::Delta SmoothedRtt() OVERRIDE;
  // End implementation of SendAlgorithmInterface.

 private:
  void EstimateDelayBandwidth(QuicTime feedback_receive_time,
                              QuicBandwidth sent_bandwidth);
  void EstimateNewBandwidth(QuicTime feedback_receive_time,
                            QuicBandwidth sent_bandwidth);
  void EstimateNewBandwidthAfterDraining(
      QuicTime feedback_receive_time,
      QuicTime::Delta estimated_congestion_delay);
  void EstimateBandwidthAfterLossEvent(QuicTime feedback_receive_time);
  void EstimateBandwidthAfterDelayEvent(
      QuicTime feedback_receive_time,
      QuicTime::Delta estimated_congestion_delay);
  void ResetCurrentBandwidth(QuicTime feedback_receive_time,
                             QuicBandwidth new_rate);
  bool ProbingPhase(QuicTime feedback_receive_time);

  bool probing_;  // Are we currently in the probing phase?
  QuicBandwidth current_bandwidth_;
  QuicTime::Delta smoothed_rtt_;
  scoped_ptr<ChannelEstimator> channel_estimator_;
  scoped_ptr<InterArrivalBitrateRampUp> bitrate_ramp_up_;
  scoped_ptr<InterArrivalOveruseDetector> overuse_detector_;
  scoped_ptr<InterArrivalProbe> probe_;
  scoped_ptr<InterArrivalStateMachine> state_machine_;
  scoped_ptr<PacedSender> paced_sender_;
  int accumulated_number_of_lost_packets_;
  BandwidthUsage bandwidth_usage_state_;
  QuicTime back_down_time_;  // Time when we decided to back down.
  QuicBandwidth back_down_bandwidth_;  // Bandwidth before backing down.
  QuicTime::Delta back_down_congestion_delay_;  // Delay when backing down.

  DISALLOW_COPY_AND_ASSIGN(InterArrivalSender);
};

}  // namespace net
#endif  // NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_SENDER_H_
