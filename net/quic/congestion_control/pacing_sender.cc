// Copyright 2013 The Chromium Authors. All rights reserved.
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
      max_segment_size_(kDefaultMaxPacketSize) {
}

PacingSender::~PacingSender() {}

void PacingSender::SetFromConfig(const QuicConfig& config, bool is_server) {
  max_segment_size_ = config.server_max_packet_size();
  sender_->SetFromConfig(config, is_server);
}

void PacingSender::OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& feedback,
      QuicTime feedback_receive_time,
      const SendAlgorithmInterface::SentPacketsMap& sent_packets) {
  sender_->OnIncomingQuicCongestionFeedbackFrame(
      feedback, feedback_receive_time, sent_packets);
}

void PacingSender::OnIncomingAck(
    QuicPacketSequenceNumber acked_sequence_number,
    QuicByteCount acked_bytes,
    QuicTime::Delta rtt) {
  sender_->OnIncomingAck(acked_sequence_number, acked_bytes, rtt);
}

void PacingSender::OnIncomingLoss(QuicPacketSequenceNumber largest_loss,
                                  QuicTime ack_receive_time) {
  sender_->OnIncomingLoss(largest_loss, ack_receive_time);
}

bool PacingSender::OnPacketSent(QuicTime sent_time,
                                QuicPacketSequenceNumber sequence_number,
                                QuicByteCount bytes,
                                TransmissionType transmission_type,
                                HasRetransmittableData is_retransmittable) {
  // The next packet should be sent as soon as the current packets has
  // been transferred.
  next_packet_send_time_ =
      next_packet_send_time_.Add(BandwidthEstimate().TransferTime(bytes));
  return sender_->OnPacketSent(sent_time, sequence_number, bytes,
                               transmission_type, is_retransmittable);
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
  if (!time_until_send.IsZero()) {
    DCHECK(time_until_send.IsInfinite());
    // The underlying sender prevents sending.
    return time_until_send;
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
    return next_packet_send_time_.Subtract(now);
  }

  // Sent it immediately.  The epoch end will be adjusted in OnPacketSent.
  was_last_send_delayed_ = false;
  return QuicTime::Delta::Zero();
}

QuicBandwidth PacingSender::BandwidthEstimate() {
  return sender_->BandwidthEstimate();
}

QuicTime::Delta PacingSender::SmoothedRtt() {
  return sender_->SmoothedRtt();
}

QuicTime::Delta PacingSender::RetransmissionDelay() {
  return sender_->RetransmissionDelay();
}

QuicByteCount PacingSender::GetCongestionWindow() const {
  return sender_->GetCongestionWindow();
}

void PacingSender::SetCongestionWindow(QuicByteCount window) {
  sender_->SetCongestionWindow(window);
}

}  // namespace net
