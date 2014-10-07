// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_CLOCK_DEVICE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_CLOCK_DEVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"

namespace chromecast {
namespace media {

// MediaClockDevice -
//
// State machine:
//                      -------------------
//                      |                 |
//                      v                 |
// kUninitialized --> kIdle  --------- kRunning
//
// {any state} --> kError
//
// Notes:
// - Hardware resources are acquired when transitioning from the
//   |kUninitialized| state to the |kIdle| state.
// - The initial value of the timeline can only be set in the kIdle state.
//
class MediaClockDevice
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  enum State {
    kStateUninitialized,
    kStateIdle,
    kStateRunning,
    kStateError,
  };

  // Return true if transition from |state1| to |state2| is a valid state
  // transition.
  static bool IsValidStateTransition(State state1, State state2);

  // Returns string representation of state (for logging)
  static std::string StateToString(const State& state);

  MediaClockDevice();
  virtual ~MediaClockDevice();

  // Return the current state of the media clock.
  virtual State GetState() const = 0;

  // Changes the state and performs any necessary transitions.
  // Returns true when successful.
  virtual bool SetState(State new_state) = 0;

  // Sets the initial value of the timeline.
  // Can only be invoked in state kStateIdle.
  // Returns true when successful.
  virtual bool ResetTimeline(base::TimeDelta time) = 0;

  // Sets the clock rate.
  // Setting it to 0 means the clock is not progressing and that the renderer
  // tied to this media clock should pause rendering.
  // Can only be invoked in states kStateIdle or kStateRunning.
  virtual bool SetRate(float rate) = 0;

  // Retrieves the media clock time.
  // Can only be invoked in states kStateIdle or kStateRunning.
  virtual base::TimeDelta GetTime() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaClockDevice);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_CLOCK_DEVICE_H_
