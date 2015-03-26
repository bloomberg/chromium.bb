// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TIME_SOURCE_H_
#define MEDIA_BASE_TIME_SOURCE_H_

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// A TimeSource is capable of providing the current media time.
class MEDIA_EXPORT TimeSource {
 public:
  TimeSource() {}
  virtual ~TimeSource() {}

  // Signal the time source to start ticking. It is expected that values from
  // CurrentMediaTime() will start increasing.
  virtual void StartTicking() = 0;

  // Signal the time source to stop ticking. It is expected that values from
  // CurrentMediaTime() will remain constant.
  virtual void StopTicking() = 0;

  // Updates the current playback rate. It is expected that values from
  // CurrentMediaTime() will eventually reflect the new playback rate (e.g., the
  // media time will advance at half speed if the rate was set to 0.5f).
  virtual void SetPlaybackRate(float playback_rate) = 0;

  // Sets the media time to start ticking from. Only valid to call while the
  // time source is not ticking.
  virtual void SetMediaTime(base::TimeDelta time) = 0;

  // Returns the current media timestamp relative to the timestamp set by
  // SetMediaTime().
  //
  // Values returned are intended for informational purposes, such as displaying
  // UI with the current minute and second count. While it is guaranteed values
  // will never go backwards, the frequency at which they update may be low.
  virtual base::TimeDelta CurrentMediaTime() = 0;

  // Converts a media timestamp into a wall clock time. If the media time is
  // stopped, returns a null TimeTicks.
  //
  // |media_time| values too far ahead of the current media time will return an
  // estimated value; as such, these values may go backwards in time slightly.
  //
  // |media_time| values behind the current media time may be significantly
  // incorrect if the playback rate has changed recently. The only guarantee is
  // that the returned time will be less than the current wall clock time.
  virtual base::TimeTicks GetWallClockTime(base::TimeDelta media_time) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_TIME_SOURCE_H_
