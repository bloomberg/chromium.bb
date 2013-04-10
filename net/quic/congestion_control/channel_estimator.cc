// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/channel_estimator.h"

// To get information about bandwidth, our send rate for a pair of packets must
// be much faster (ideally back to back) than the receive rate. In that
// scenario, the arriving packet pair will tend to arrive at the max bandwidth
// of the channel. Said another way, when our inter-departure time is a small
// fraction of the inter-arrival time for the same pair of packets, then we can
// get an estimate of bandwidth from that interarrival time. The following
// constant is the threshold ratio for deriving bandwidth information.
static const int kInterarrivalRatioThresholdForBandwidthEstimation = 5;
static const size_t kMinNumberOfSamples = 10;
static const size_t kMaxNumberOfSamples = 100;

namespace net {

ChannelEstimator::ChannelEstimator()
    : last_sequence_number_(0),
      last_send_time_(QuicTime::Zero()),
      last_receive_time_(QuicTime::Zero()),
      sorted_bitrate_estimates_(kMaxNumberOfSamples) {
}

ChannelEstimator::~ChannelEstimator() {
}

void ChannelEstimator::OnAcknowledgedPacket(
    QuicPacketSequenceNumber sequence_number,
    QuicByteCount packet_size,
    QuicTime send_time,
    QuicTime receive_time) {
  if (last_sequence_number_ > sequence_number) {
    // Old packet. The sequence_number use the full 64 bits even though it's
    // less on the wire.
    return;
  }
  if (last_sequence_number_ != sequence_number - 1) {
    DLOG(INFO) << "Skip channel estimator due to lost packet(s)";
  } else if (last_send_time_.IsInitialized()) {
    QuicTime::Delta sent_delta = send_time.Subtract(last_send_time_);
    QuicTime::Delta received_delta = receive_time.Subtract(last_receive_time_);
    if (received_delta.ToMicroseconds() >
        kInterarrivalRatioThresholdForBandwidthEstimation *
            sent_delta.ToMicroseconds()) {
      UpdateFilter(received_delta, packet_size, sequence_number);
    }
  }
  last_sequence_number_ = sequence_number;
  last_send_time_ = send_time;
  last_receive_time_ = receive_time;
}

ChannelEstimateState ChannelEstimator::GetChannelEstimate(
    QuicBandwidth* estimate) const {
  if (sorted_bitrate_estimates_.Size() < kMinNumberOfSamples) {
    // Not enough data to make an estimate.
    return kChannelEstimateUnknown;
  }
  // Our data is stored in a sorted map, we need to iterate through our map to
  // find the estimated bitrates at our targeted percentiles.

  // Calculate 25th percentile.
  size_t beginning_window = sorted_bitrate_estimates_.Size() / 4;
  // Calculate 50th percentile.
  size_t median = sorted_bitrate_estimates_.Size() / 2;
  // Calculate 75th percentile.
  size_t end_window = sorted_bitrate_estimates_.Size() - beginning_window;

  QuicBandwidth bitrate_25th_percentile = QuicBandwidth::Zero();
  QuicBandwidth median_bitrate = QuicBandwidth::Zero();
  QuicBandwidth bitrate_75th_percentile = QuicBandwidth::Zero();
  QuicMaxSizedMap<QuicBandwidth, QuicPacketSequenceNumber>::ConstIterator it =
      sorted_bitrate_estimates_.Begin();

  for (size_t i = 0; i <= end_window; ++i, ++it) {
    DCHECK(it != sorted_bitrate_estimates_.End());
    if (i == beginning_window) {
      bitrate_25th_percentile = it->first;
    }
    if (i == median) {
      median_bitrate = it->first;
    }
    if (i == end_window) {
      bitrate_75th_percentile = it->first;
    }
  }
  *estimate = median_bitrate;
  DLOG(INFO) << "Channel estimate is:"
             << median_bitrate.ToKBitsPerSecond() << " Kbit/s";
  // If the bitrates in our 25th to 75th percentile window varies more than
  // 25% of the median bitrate we consider the estimate to be uncertain.
  if (bitrate_75th_percentile.Subtract(bitrate_25th_percentile) >
      median_bitrate.Scale(0.25f)) {
    return kChannelEstimateUncertain;
  }
  return kChannelEstimateGood;
}

void ChannelEstimator::UpdateFilter(QuicTime::Delta received_delta,
                                    QuicByteCount size_delta,
                                    QuicPacketSequenceNumber sequence_number) {
  QuicBandwidth estimate =
      QuicBandwidth::FromBytesAndTimeDelta(size_delta, received_delta);
  sorted_bitrate_estimates_.Insert(estimate, sequence_number);
}

}  // namespace net
