// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_timestamp_helper.h"

namespace media {
namespace cast {

RtpTimestampHelper::RtpTimestampHelper(int frequency)
    : frequency_(frequency),
      last_rtp_timestamp_(0) {
}

RtpTimestampHelper::~RtpTimestampHelper() {
}

bool RtpTimestampHelper::GetCurrentTimeAsRtpTimestamp(
    const base::TimeTicks& now, uint32* rtp_timestamp) const {
  if (last_capture_time_.is_null())
    return false;
  base::TimeDelta elapsed_time = now - last_capture_time_;
  *rtp_timestamp = last_rtp_timestamp_ + elapsed_time.InMilliseconds() *
      frequency_ / base::Time::kMillisecondsPerSecond;
  return true;
}

void RtpTimestampHelper::StoreLatestTime(
    base::TimeTicks capture_time, uint32 rtp_timestamp) {
  last_capture_time_ = capture_time;
  last_rtp_timestamp_ = rtp_timestamp;
}

}  // namespace cast
}  // namespace media
