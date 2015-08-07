// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_MEDIA_CLOCK_DEVICE_H_
#define CHROMECAST_PUBLIC_MEDIA_MEDIA_CLOCK_DEVICE_H_

#include <string>

namespace chromecast {
namespace media {

// Interface for platform-specific pipeline clock.
// Pipeline clocks follow this state machine:
//                      -------------------
//                      |                 |
//                      v                 |
// kUninitialized --> kIdle  --------- kRunning
//
// {any state} --> kError
//
// Notes:
// - Hardware resources should be acquired when transitioning from the
//   |kUninitialized| state to the |kIdle| state.
// - The initial value of the timeline will only be set in the kIdle state.
class MediaClockDevice {
 public:
  enum State {
    kStateUninitialized,
    kStateIdle,
    kStateRunning,
    kStateError,
  };

  virtual ~MediaClockDevice() {}

  // Returns the current state of the media clock.
  virtual State GetState() const = 0;

  // Changes the state and performs any necessary transitions.
  // Returns true when successful.
  virtual bool SetState(State new_state) = 0;

  // Sets the initial value of the timeline in microseconds.
  // Will only be invoked in state kStateIdle.
  // Returns true when successful.
  virtual bool ResetTimeline(int64_t time_microseconds) = 0;

  // Sets the clock rate.
  // |rate| == 0 means the clock is not progressing and that the renderer
  // tied to this media clock should pause rendering.
  // Will only be invoked in states kStateIdle or kStateRunning.
  virtual bool SetRate(float rate) = 0;

  // Retrieves the media clock time in microseconds.
  // Will only be invoked in states kStateIdle or kStateRunning.
  virtual int64_t GetTimeMicroseconds() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_CLOCK_DEVICE_H_
