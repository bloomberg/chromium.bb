// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/inter_arrival_bitrate_ramp_up.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/quic/congestion_control/cube_root.h"
#include "net/quic/quic_protocol.h"

namespace {
// The following constants are in 2^10 fractions of a second instead of ms to
// allow a 10 shift right to divide.
const int kCubeScale = 40;  // 1024*1024^3 (first 1024 is from 0.100^3)
                            // where 0.100 is 100 ms which is the scaling
                            // round trip time.
// TODO(pwestin): Tuning parameter, currently close to TCP cubic at 100ms RTT.
const int kPacedCubeScale = 6000;
const uint64 kCubeFactor = (GG_UINT64_C(1) << kCubeScale) / kPacedCubeScale;
}  // namespace

namespace net {

InterArrivalBitrateRampUp::InterArrivalBitrateRampUp(const QuicClock* clock)
    : clock_(clock),
      current_rate_(QuicBandwidth::Zero()),
      channel_estimate_(QuicBandwidth::Zero()),
      available_channel_estimate_(QuicBandwidth::Zero()),
      halfway_point_(QuicBandwidth::Zero()),
      epoch_(QuicTime::Zero()),
      last_update_time_(QuicTime::Zero()) {
}

void InterArrivalBitrateRampUp::Reset(QuicBandwidth new_rate,
                                      QuicBandwidth available_channel_estimate,
                                      QuicBandwidth channel_estimate) {
  epoch_ = clock_->ApproximateNow();
  last_update_time_ = epoch_;
  available_channel_estimate_ = std::max(new_rate, available_channel_estimate);
  channel_estimate_ = std::max(channel_estimate, available_channel_estimate_);

  halfway_point_ = available_channel_estimate_.Add(
      (channel_estimate_.Subtract(available_channel_estimate_)).Scale(0.5f));

  if (new_rate < available_channel_estimate_) {
    time_to_origin_point_ = CalcuateTimeToOriginPoint(
        available_channel_estimate_.Subtract(new_rate));
  } else if (new_rate >= channel_estimate_) {
    time_to_origin_point_ = 0;
  } else if (new_rate >= halfway_point_) {
    time_to_origin_point_ =
        CalcuateTimeToOriginPoint(channel_estimate_.Subtract(new_rate));
  } else {
    time_to_origin_point_ = CalcuateTimeToOriginPoint(
        new_rate.Subtract(available_channel_estimate_));
  }
  current_rate_ = new_rate;
  DLOG(INFO) << "Reset; time to origin point:" << time_to_origin_point_;
}

void InterArrivalBitrateRampUp::UpdateChannelEstimate(
    QuicBandwidth channel_estimate) {
  if (available_channel_estimate_ > channel_estimate ||
      current_rate_ > channel_estimate ||
      channel_estimate_ == channel_estimate) {
    // Ignore, because one of the following reasons:
    // 1) channel estimate is bellow our current available estimate which we
    //    value higher that this estimate.
    // 2) channel estimate is bellow our current send rate.
    // 3) channel estimate has not changed.
    return;
  }
  if (available_channel_estimate_ == halfway_point_ &&
      channel_estimate_  == halfway_point_) {
    // First time we get a usable channel estimate.
    channel_estimate_ = channel_estimate;
    halfway_point_ = available_channel_estimate_.Add(
        (channel_estimate_.Subtract(available_channel_estimate_).Scale(0.5f)));
    DLOG(INFO) << "UpdateChannelEstimate; first usable value:"
               << channel_estimate.ToKBitsPerSecond() << " Kbits/s";
    return;
  }
  if (current_rate_ < halfway_point_) {
    // Update channel estimate without recalculating if we are bellow the
    // halfway point.
    channel_estimate_ = channel_estimate;
    return;
  }
  // We are between halfway point and our channel_estimate.
  epoch_ = clock_->ApproximateNow();
  last_update_time_ = epoch_;
  channel_estimate_ = channel_estimate;

  time_to_origin_point_ =
      CalcuateTimeToOriginPoint(channel_estimate_.Subtract(current_rate_));

  DLOG(INFO) << "UpdateChannelEstimate; time to origin point:"
             << time_to_origin_point_;
}

QuicBandwidth InterArrivalBitrateRampUp::GetNewBitrate(
    QuicBandwidth sent_bitrate) {
  DCHECK(epoch_.IsInitialized());
  QuicTime current_time = clock_->ApproximateNow();
  // Cubic is "independent" of RTT, the update is limited by the time elapsed.
  if (current_time.Subtract(last_update_time_) <= MaxCubicTimeInterval()) {
    return current_rate_;
  }
  QuicTime::Delta time_from_last_update =
      current_time.Subtract(last_update_time_);

  last_update_time_ = current_time;

  if (!sent_bitrate.IsZero() &&
      sent_bitrate.Add(sent_bitrate) < current_rate_) {
    // Don't go up in bitrate when we are not sending.
    // We need to update the epoch to reflect this state.
    epoch_ = epoch_.Add(time_from_last_update);
    DLOG(INFO) << "Don't increase; our sent bitrate is:"
               << sent_bitrate.ToKBitsPerSecond() << " Kbits/s"
               << " current target rate is:"
               << current_rate_.ToKBitsPerSecond() << " Kbits/s";
    return current_rate_;
  }
  QuicTime::Delta time_from_epoch = current_time.Subtract(epoch_);

  // Change the time unit from microseconds to 2^10 fractions per second. This
  // is done to allow us to use shift as a divide operator.
  int64 elapsed_time = (time_from_epoch.ToMicroseconds() << 10) /
      kNumMicrosPerSecond;

  int64 offset = time_to_origin_point_ - elapsed_time;
  // Note: using int64 since QuicBandwidth can't be negative
  int64 delta_pace_kbps = (kPacedCubeScale * offset * offset * offset) >>
        kCubeScale;

  bool start_bellow_halfway_point = false;
  if (current_rate_ < halfway_point_) {
    start_bellow_halfway_point = true;

    // available_channel_estimate_ is the orgin of the cubic function.
    QuicBandwidth current_rate = QuicBandwidth::FromBytesPerSecond(
        available_channel_estimate_.ToBytesPerSecond() -
            (delta_pace_kbps << 10));

    if (start_bellow_halfway_point && current_rate >= halfway_point_) {
      // We passed the halfway point, recalculate with new orgin.
      epoch_ = clock_->ApproximateNow();
      // channel_estimate_ is the new orgin of the cubic function.
      if (current_rate >= channel_estimate_) {
        time_to_origin_point_ = 0;
      } else {
        time_to_origin_point_ =
            CalcuateTimeToOriginPoint(channel_estimate_.Subtract(current_rate));
      }
      DLOG(INFO) << "Passed the halfway point; time to origin point:"
                 << time_to_origin_point_;
    }
    current_rate_ = current_rate;
  } else {
    // channel_estimate_ is the orgin of the cubic function.
    current_rate_ = QuicBandwidth::FromBytesPerSecond(
        channel_estimate_.ToBytesPerSecond() - (delta_pace_kbps << 10));
  }
  return current_rate_;
}

uint32 InterArrivalBitrateRampUp::CalcuateTimeToOriginPoint(
    QuicBandwidth rate_difference) const {
  return CubeRoot::Root(kCubeFactor * rate_difference.ToKBytesPerSecond());
}

}  // namespace net
