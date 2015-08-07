// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_CLOCK_DEVICE_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_CLOCK_DEVICE_DEFAULT_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromecast/public/media/media_clock_device.h"

namespace chromecast {
namespace media {

class MediaClockDeviceDefault : public MediaClockDevice {
 public:
  MediaClockDeviceDefault();
  ~MediaClockDeviceDefault() override;

  // MediaClockDevice implementation.
  State GetState() const override;
  bool SetState(State new_state) override;
  bool ResetTimeline(int64_t time_microseconds) override;
  bool SetRate(float rate) override;
  int64_t GetTimeMicroseconds() override;

 private:
  State state_;

  // Media time sampled at STC time |stc_|.
  base::TimeDelta media_time_;
  base::TimeTicks stc_;

  float rate_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaClockDeviceDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_CLOCK_DEVICE_DEFAULT_H_
