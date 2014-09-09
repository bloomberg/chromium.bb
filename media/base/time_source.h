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

  // Returns the current media time.
  //
  // Values returned are intended for informational purposes, such as displaying
  // UI with the current minute and second count. While it is guaranteed values
  // will never go backwards, the frequency at which they update may be low.
  virtual base::TimeDelta CurrentMediaTime() = 0;

  // Returns the current media time for use with synchronizing video.
  //
  // Differences from CurrentMediaTime():
  //   - Values returned update at a much higher frequency (e.g., suitable for
  //     playback of 60 FPS content).
  //   - As a result, values may go slightly backwards depending on the
  //     implementation (e.g., uses interpolation).
  //
  // TODO(scherkus): Replace with a method that returns wall clock time for a
  // given media time for use with VideoFrameScheduler http://crbug.com/110814
  virtual base::TimeDelta CurrentMediaTimeForSyncingVideo() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_TIME_SOURCE_H_
