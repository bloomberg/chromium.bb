// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/congestion_control/congestion_control.h"

#include "base/logging.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"

namespace media {
namespace cast {

static const int64 kCongestionControlMinChangeIntervalMs = 10;
static const int64 kCongestionControlMaxChangeIntervalMs = 100;

// At 10 ms RTT TCP Reno would ramp 1500 * 8 * 100  = 1200 Kbit/s.
// NACK is sent after a maximum of 10 ms.
static const int kCongestionControlMaxBitrateIncreasePerMillisecond = 1200;

static const int64 kMaxElapsedTimeMs = kCongestionControlMaxChangeIntervalMs;

CongestionControl::CongestionControl(base::TickClock* clock,
                                     float congestion_control_back_off,
                                     uint32 max_bitrate_configured,
                                     uint32 min_bitrate_configured,
                                     uint32 start_bitrate)
    : clock_(clock),
      congestion_control_back_off_(congestion_control_back_off),
      max_bitrate_configured_(max_bitrate_configured),
      min_bitrate_configured_(min_bitrate_configured),
      bitrate_(start_bitrate) {
  DCHECK_GT(congestion_control_back_off, 0.0f) << "Invalid config";
  DCHECK_LT(congestion_control_back_off, 1.0f) << "Invalid config";
  DCHECK_GE(max_bitrate_configured, min_bitrate_configured) << "Invalid config";
  DCHECK_GE(max_bitrate_configured, start_bitrate) << "Invalid config";
  DCHECK_GE(start_bitrate, min_bitrate_configured) << "Invalid config";
}

CongestionControl::~CongestionControl() {}

bool CongestionControl::OnAck(base::TimeDelta rtt, uint32* new_bitrate) {
  base::TimeTicks now = clock_->NowTicks();

  // First feedback?
  if (time_last_increase_.is_null()) {
    time_last_increase_ = now;
    time_last_decrease_ = now;
    return false;
  }
  // Are we at the max bitrate?
  if (max_bitrate_configured_ == bitrate_)
    return false;

  // Make sure RTT is never less than 1 ms.
  rtt = std::max(rtt, base::TimeDelta::FromMilliseconds(1));

  base::TimeDelta elapsed_time =
      std::min(now - time_last_increase_,
               base::TimeDelta::FromMilliseconds(kMaxElapsedTimeMs));
  base::TimeDelta change_interval = std::max(
      rtt,
      base::TimeDelta::FromMilliseconds(kCongestionControlMinChangeIntervalMs));
  change_interval = std::min(
      change_interval,
      base::TimeDelta::FromMilliseconds(kCongestionControlMaxChangeIntervalMs));

  // Have enough time have passed?
  if (elapsed_time < change_interval)
    return false;

  time_last_increase_ = now;

  // One packet per RTT multiplied by the elapsed time fraction.
  // 1500 * 8 * (1000 / rtt_ms) * (elapsed_time_ms / 1000) =>
  // 1500 * 8 * elapsed_time_ms / rtt_ms.
  uint32 bitrate_increase =
      (1500 * 8 * elapsed_time.InMilliseconds()) / rtt.InMilliseconds();
  uint32 max_bitrate_increase =
      kCongestionControlMaxBitrateIncreasePerMillisecond *
      elapsed_time.InMilliseconds();
  bitrate_increase = std::min(max_bitrate_increase, bitrate_increase);
  *new_bitrate = std::min(bitrate_increase + bitrate_, max_bitrate_configured_);
  bitrate_ = *new_bitrate;
  return true;
}

bool CongestionControl::OnNack(base::TimeDelta rtt, uint32* new_bitrate) {
  base::TimeTicks now = clock_->NowTicks();

  // First feedback?
  if (time_last_decrease_.is_null()) {
    time_last_increase_ = now;
    time_last_decrease_ = now;
    return false;
  }
  base::TimeDelta elapsed_time =
      std::min(now - time_last_decrease_,
               base::TimeDelta::FromMilliseconds(kMaxElapsedTimeMs));
  base::TimeDelta change_interval = std::max(
      rtt,
      base::TimeDelta::FromMilliseconds(kCongestionControlMinChangeIntervalMs));
  change_interval = std::min(
      change_interval,
      base::TimeDelta::FromMilliseconds(kCongestionControlMaxChangeIntervalMs));

  // Have enough time have passed?
  if (elapsed_time < change_interval)
    return false;

  time_last_decrease_ = now;
  time_last_increase_ = now;

  *new_bitrate =
      std::max(static_cast<uint32>(bitrate_ * congestion_control_back_off_),
               min_bitrate_configured_);

  bitrate_ = *new_bitrate;
  return true;
}

}  // namespace cast
}  // namespace media
