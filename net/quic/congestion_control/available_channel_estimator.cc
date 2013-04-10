// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/available_channel_estimator.h"

static const int kNumberOfSamples = 9;

namespace net {

AvailableChannelEstimator::AvailableChannelEstimator(
    QuicPacketSequenceNumber sequence_number,
    QuicTime first_send_time,
    QuicTime first_receive_time)
    : first_sequence_number_(sequence_number),
      first_send_time_(first_send_time),
      first_receive_time_(first_receive_time),
      last_incorporated_sequence_number_(sequence_number),
      last_time_sent_(QuicTime::Zero()),
      last_receive_time_(QuicTime::Zero()),
      number_of_sequence_numbers_(0),
      received_bytes_(0) {
}

void AvailableChannelEstimator::OnIncomingFeedback(
    QuicPacketSequenceNumber sequence_number,
    QuicByteCount packet_size,
    QuicTime sent_time,
    QuicTime receive_time) {
  if (sequence_number <= first_sequence_number_) {
    // Ignore pre-probe feedback.
    return;
  }
  if (sequence_number <= last_incorporated_sequence_number_) {
    // Ignore old feedback; will remove duplicates.
    return;
  }
  // Remember the highest received sequence number.
  last_incorporated_sequence_number_ = sequence_number;
  if (number_of_sequence_numbers_ < kNumberOfSamples) {
    // We don't care how many sequence numbers we have after we pass
    // kNumberOfSamples.
    number_of_sequence_numbers_++;
  }
  last_receive_time_ = receive_time;
  last_time_sent_ = sent_time;
  received_bytes_ += packet_size;
  // TODO(pwestin): the variance here should give us information about accuracy.
}

AvailableChannelEstimateState
    AvailableChannelEstimator::GetAvailableChannelEstimate(
        QuicBandwidth* bandwidth) const {
  if (number_of_sequence_numbers_ < 2) {
    return kAvailableChannelEstimateUnknown;
  }
  QuicTime::Delta send_delta = last_time_sent_.Subtract(first_send_time_);
  QuicTime::Delta receive_delta =
      last_receive_time_.Subtract(first_receive_time_);

  // TODO(pwestin): room for improvement here. Keeping it simple for now.
  *bandwidth = QuicBandwidth::FromBytesAndTimeDelta(received_bytes_,
                                                    receive_delta);

  QuicTime::Delta diff = receive_delta.Subtract(send_delta);
  QuicTime::Delta ten_percent_of_send_time =
      QuicTime::Delta::FromMicroseconds(send_delta.ToMicroseconds() / 10);

  if (diff < ten_percent_of_send_time) {
    return kAvailableChannelEstimateSenderLimited;
  }
  if (number_of_sequence_numbers_ < kNumberOfSamples) {
    return kAvailableChannelEstimateUncertain;
  }
  return kAvailableChannelEstimateGood;
}

}  // namespace net
