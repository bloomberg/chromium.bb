// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/pacing_sender.h"

namespace net {

PacingSender::PacingSender(SendAlgorithmInterface* sender,
                           QuicTime::Delta alarm_granularity)
    : sender_(sender),
      alarm_granularity_(alarm_granularity),
      next_packet_send_time_(QuicTime::Zero()),
      was_last_send_delayed_(false),
      max_segment_size_(kDefaultMaxPacketSize),
      updated_rtt_(false) {
}

PacingSender::~PacingSender() {}

void PacingSender::SetFromConfig(const QuicConfig& config, bool is_server) {
  sender_->SetFromConfig(config, is_server);
}

void PacingSender::OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& feedback,
      QuicTime feedback_receive_time) {
  sender_->OnIncomingQuicCongestionFeedbackFrame(
      feedback, feedback_receive_time);
}

void PacingSender::OnPacketAcked(
    QuicPacketSequenceNumber acked_sequence_number,
    QuicByteCount acked_bytes) {
  sender_->OnPacketAcked(acked_sequence_number, acked_bytes);
}

void PacingSender::OnPacketLost(QuicPacketSequenceNumber sequence_number,
                                QuicTime ack_receive_time) {
  sender_->OnPacketLost(sequence_number, ack_receive_time);
}

bool PacingSender::OnPacketSent(
    QuicTime sent_time,
    QuicPacketSequenceNumber sequence_number,
    QuicByteCount bytes,
    TransmissionType transmission_type,
    HasRetransmittableData has_retransmittable_data) {
  // Only pace data packets once we have an updated RTT.
  if (has_retransmittable_data == HAS_RETRANSMITTABLE_DATA && updated_rtt_) {
    // The next packet should be sent as soon as the current packets has
    // been transferred.  We pace at twice the rate of the underlying
    // sender's bandwidth estimate to help ensure that pacing doesn't become
    // a bottleneck.
    const float kPacingAggression = 2;
    QuicTime::Delta delay =
        BandwidthEstimate().Scale(kPacingAggression).TransferTime(bytes);
    next_packet_send_time_ = next_packet_send_time_.Add(delay);
  }
  return sender_->OnPacketSent(sent_time, sequence_number, bytes,
                               transmission_type, has_retransmittable_data);
}

void PacingSender::OnRetransmissionTimeout(bool packets_retransmitted) {
  sender_->OnRetransmissionTimeout(packets_retransmitted);
}

void PacingSender::OnPacketAbandoned(QuicPacketSequenceNumber sequence_number,
                                     QuicByteCount abandoned_bytes) {
  sender_->OnPacketAbandoned(sequence_number, abandoned_bytes);
}

QuicTime::Delta PacingSender::TimeUntilSend(
      QuicTime now,
      TransmissionType transmission_type,
      HasRetransmittableData has_retransmittable_data,
      IsHandshake handshake) {
  QuicTime::Delta time_until_send =
      sender_->TimeUntilSend(now, transmission_type,
                             has_retransmittable_data, handshake);
  if (!updated_rtt_) {
    // Don't pace if we don't have an updated RTT estimate.
    return time_until_send;
  }

  if (!time_until_send.IsZero()) {
    DCHECK(time_until_send.IsInfinite());
    // The underlying sender prevents sending.
    return time_until_send;
  }

  if (has_retransmittable_data == NO_RETRANSMITTABLE_DATA) {
    // Don't pace ACK packets, since they do not count against CWND and do not
    // cause CWND to grow.
    return QuicTime::Delta::Zero();
  }

  if (!was_last_send_delayed_ &&
      (!next_packet_send_time_.IsInitialized() ||
       now > next_packet_send_time_.Add(alarm_granularity_))) {
    // An alarm did not go off late, instead the application is "slow"
    // delivering data.  In this case, we restrict the amount of lost time
    // that we can make up for.
    next_packet_send_time_ = now.Subtract(alarm_granularity_);
  }

  // If the end of the epoch is far enough in the future, delay the send.
  if (next_packet_send_time_ > now.Add(alarm_granularity_)) {
    was_last_send_delayed_ = true;
    DVLOG(1) << "Delaying packet: "
             << next_packet_send_time_.Subtract(now).ToMicroseconds();
    return next_packet_send_time_.Subtract(now);
  }

  // Sent it immediately.  The epoch end will be adjusted in OnPacketSent.
  was_last_send_delayed_ = false;
  DVLOG(1) << "Sending packet now";
  return QuicTime::Delta::Zero();
}

QuicBandwidth PacingSender::BandwidthEstimate() const {
  return sender_->BandwidthEstimate();
}

void PacingSender::UpdateRtt(QuicTime::Delta rtt_sample) {
  updated_rtt_= true;
  sender_->UpdateRtt(rtt_sample);
}

QuicTime::Delta PacingSender::SmoothedRtt() const {
  return sender_->SmoothedRtt();
}

QuicTime::Delta PacingSender::RetransmissionDelay() const {
  return sender_->RetransmissionDelay();
}

QuicByteCount PacingSender::GetCongestionWindow() const {
  return sender_->GetCongestionWindow();
}

}  // namespace net
