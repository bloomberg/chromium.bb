// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/inter_arrival_overuse_detector.h"

#include <math.h>
#include <stdlib.h>

// Initial noise variance, equal to a standard deviation of 1 millisecond.
static const float kInitialVarianceNoise = 1000000.0;

// Minimum variance of the time delta.
static const int kMinVarianceDelta = 10000;

// Threshold for accumulated delta.
static const int kThresholdAccumulatedDeltasUs = 1000;

// The higher the beta parameter, the lower is the effect of the input and the
// more damping of the noise. And the longer time for a detection.
static const float kBeta = 0.98f;

// Same as above, described as numerator and denominator.
static const int kBetaNumerator = 49;
static const int kBetaDenominator = 50;

// Trigger a signal when the accumulated time drift is larger than
// 5 x standard deviation.
// A lower value triggers earlier with more false detect as a side effect.
static const int kDetectDriftStandardDeviation = 5;

// Trigger an overuse when the time difference between send time and receive
// is larger than 7 x standard deviation.
// A lower value triggers earlier with more false detect as a side effect.
static const float kDetectTimeDiffStandardDeviation = 7;

// Trigger an overuse when the mean of the time difference diverges too far
// from 0.
// A higher value trigger earlier with more false detect as a side effect.
static const int kDetectSlopeFactor = 14;

// We need to get some initial statistics before the detection can start.
static const int kMinSamplesBeforeDetect = 10;

namespace net {

InterArrivalOveruseDetector::InterArrivalOveruseDetector()
    : last_sequence_number_(0),
      num_of_deltas_(0),
      accumulated_deltas_(QuicTime::Delta::Zero()),
      delta_mean_(0.0),
      delta_variance_(kInitialVarianceNoise),
      delta_overuse_counter_(0),
      delta_estimate_(kBandwidthSteady),
      slope_overuse_counter_(0),
      slope_estimate_(kBandwidthSteady),
      send_receive_offset_(QuicTime::Delta::Infinite()),
      estimated_congestion_delay_(QuicTime::Delta::Zero()) {
}

void InterArrivalOveruseDetector::OnAcknowledgedPacket(
    QuicPacketSequenceNumber sequence_number,
    QuicTime send_time,
    bool last_of_send_time,
    QuicTime receive_time) {
  if (last_sequence_number_ >= sequence_number) {
    // This is an old packet and should be ignored.  Note that we are called
    // with a full 64 bit sequence number, even if the wire format may only
    // convey some low-order bits of that number.
    DLOG(INFO) << "Skip old packet";
    return;
  }

  last_sequence_number_ = sequence_number;

  if (current_packet_group_.send_time != send_time) {
    // First value in this group. If the last packet of a group is lost we
    // overwrite the old value and start over with a new measurement.
    current_packet_group_.send_time = send_time;
    // The receive_time value might not be first in a packet burst if that
    // packet was lost, however we will still use it in this calculation.
    UpdateSendReceiveTimeOffset(receive_time.Subtract(send_time));
  }
  if (!last_of_send_time) {
    // We expect more packet with this send time.
    return;
  }
  // First packet of a later group, the previous group sample is ready.
  if (previous_packet_group_.send_time.IsInitialized()) {
    QuicTime::Delta sent_delta = send_time.Subtract(
        previous_packet_group_.send_time);
    QuicTime::Delta receive_delta = receive_time.Subtract(
        previous_packet_group_.last_receive_time);
    // We assume that groups of packets are sent together as bursts (tagged
    // with identical send times) because it is too computationally expensive
    // to pace them out individually.  The received_delta is then the total
    // delta between the receipt of the last packet of the previous group, and
    // the last packet of the current group.  Assuming we are transmitting
    // these bursts on average at the available bandwidth rate, there should be
    // no change in overall spread (both deltas should be the same).
    UpdateFilter(receive_delta, sent_delta);
  }
  // Save current as previous.
  previous_packet_group_ = current_packet_group_;
  previous_packet_group_.last_receive_time = receive_time;
}

void InterArrivalOveruseDetector::UpdateSendReceiveTimeOffset(
    QuicTime::Delta offset) {
  // Note the send and receive time can have a randomly large offset, however
  // they are stable in relation to each other, hence no or extremely low clock
  // drift relative to the duration of our stream.
  if (offset.ToMicroseconds() < send_receive_offset_.ToMicroseconds()) {
    send_receive_offset_ = offset;
  }
  estimated_congestion_delay_ = offset.Subtract(send_receive_offset_);
}

BandwidthUsage InterArrivalOveruseDetector::GetState(
    QuicTime::Delta* estimated_congestion_delay) {
  *estimated_congestion_delay = estimated_congestion_delay_;
  int64 sigma_delta = sqrt(static_cast<double>(delta_variance_));
  DetectSlope(sigma_delta);
  DetectDrift(sigma_delta);
  return std::max(slope_estimate_, delta_estimate_);
}

void InterArrivalOveruseDetector::UpdateFilter(QuicTime::Delta received_delta,
                                               QuicTime::Delta sent_delta) {
  ++num_of_deltas_;
  QuicTime::Delta time_diff = received_delta.Subtract(sent_delta);
  UpdateDeltaEstimate(time_diff);
  accumulated_deltas_ = accumulated_deltas_.Add(time_diff);
}

void InterArrivalOveruseDetector::UpdateDeltaEstimate(
    QuicTime::Delta residual) {
  DCHECK_EQ(1, kBetaDenominator - kBetaNumerator);
  int64 residual_us = residual.ToMicroseconds();
  delta_mean_ =
      (kBetaNumerator * delta_mean_ + residual_us) / kBetaDenominator;
  delta_variance_ =
      (kBetaNumerator * delta_variance_ +
      (delta_mean_ - residual_us) * (delta_mean_ - residual_us)) /
      kBetaDenominator;

  if (delta_variance_ < kMinVarianceDelta) {
    delta_variance_ = kMinVarianceDelta;
  }
}

void InterArrivalOveruseDetector::DetectDrift(int64 sigma_delta) {
  // We have 2 drift detectors. The accumulate of deltas and the absolute time
  // differences.
  if (num_of_deltas_ < kMinSamplesBeforeDetect) {
    return;
  }
  if (delta_overuse_counter_ > 0 &&
      accumulated_deltas_.ToMicroseconds() > kThresholdAccumulatedDeltasUs) {
    if (delta_estimate_ != kBandwidthDraining) {
      DLOG(INFO) << "Bandwidth estimate drift: Draining buffer(s) "
                 << accumulated_deltas_.ToMilliseconds() << " ms";
      delta_estimate_ = kBandwidthDraining;
    }
    return;
  }
  if ((sigma_delta * kDetectTimeDiffStandardDeviation >
          estimated_congestion_delay_.ToMicroseconds()) &&
      (sigma_delta * kDetectDriftStandardDeviation >
          abs(accumulated_deltas_.ToMicroseconds()))) {
    if (delta_estimate_ != kBandwidthSteady) {
      DLOG(INFO) << "Bandwidth estimate drift: Steady"
                 << " mean:" << delta_mean_
                 << " sigma:" << sigma_delta
                 << " offset:" << send_receive_offset_.ToMicroseconds()
                 << " delta:" << estimated_congestion_delay_.ToMicroseconds()
                 << " drift:" << accumulated_deltas_.ToMicroseconds();
      delta_estimate_ = kBandwidthSteady;
      // Reset drift counter.
      accumulated_deltas_ = QuicTime::Delta::Zero();
      delta_overuse_counter_ = 0;
    }
    return;
  }
  if (accumulated_deltas_.ToMicroseconds() > 0) {
    if (delta_estimate_ != kBandwidthOverUsing) {
      ++delta_overuse_counter_;
      DLOG(INFO) << "Bandwidth estimate drift: Over using"
                 << " mean:" << delta_mean_
                 << " sigma:" << sigma_delta
                 << " offset:" << send_receive_offset_.ToMicroseconds()
                 << " delta:" << estimated_congestion_delay_.ToMicroseconds()
                 << " drift:" << accumulated_deltas_.ToMicroseconds();
      delta_estimate_ = kBandwidthOverUsing;
    }
  } else {
    if (delta_estimate_ != kBandwidthUnderUsing) {
      --delta_overuse_counter_;
      DLOG(INFO) << "Bandwidth estimate drift: Under using"
                 << " mean:" << delta_mean_
                 << " sigma:" << sigma_delta
                 << " offset:" << send_receive_offset_.ToMicroseconds()
                 << " delta:" << estimated_congestion_delay_.ToMicroseconds()
                 << " drift:" << accumulated_deltas_.ToMicroseconds();
      delta_estimate_ = kBandwidthUnderUsing;
    }
    // Adding decay of negative accumulated_deltas_ since it could be caused by
    // a starting with full buffers. This way we will always converge to 0.
    accumulated_deltas_ = accumulated_deltas_.Add(
        QuicTime::Delta::FromMicroseconds(sigma_delta >> 3));
  }
}

void InterArrivalOveruseDetector::DetectSlope(int64 sigma_delta) {
  // We use the mean change since it has a constant expected mean 0
  // regardless of number of packets and spread. It is also safe to use during
  // packet loss, since a lost packet only results in a missed filter update
  // not a drift.
  if (num_of_deltas_ < kMinSamplesBeforeDetect) {
    return;
  }
  if (slope_overuse_counter_ > 0 && delta_mean_ > 0) {
    if (slope_estimate_ != kBandwidthDraining) {
      DLOG(INFO) << "Bandwidth estimate slope: Draining buffer(s)";
    }
    slope_estimate_ = kBandwidthDraining;
    return;
  }
  if (sigma_delta > abs(delta_mean_) * kDetectSlopeFactor) {
    if (slope_estimate_ != kBandwidthSteady) {
      DLOG(INFO) << "Bandwidth estimate slope: Steady"
                 << " mean:" << delta_mean_
                 << " sigma:" << sigma_delta;
      slope_overuse_counter_ = 0;
      slope_estimate_ = kBandwidthSteady;
    }
    return;
  }
  if (delta_mean_ > 0) {
    if (slope_estimate_ != kBandwidthOverUsing) {
      ++slope_overuse_counter_;
      DLOG(INFO) << "Bandwidth estimate slope: Over using"
                 << " mean:" << delta_mean_
                 << " sigma:" << sigma_delta;
      slope_estimate_ = kBandwidthOverUsing;
    }
  } else {
    if (slope_estimate_ != kBandwidthUnderUsing) {
      --slope_overuse_counter_;
      DLOG(INFO) << "Bandwidth estimate slope: Under using"
                 << " mean:" << delta_mean_
                 << " sigma:" << sigma_delta;
      slope_estimate_ = kBandwidthUnderUsing;
    }
  }
}

}  // namespace net
