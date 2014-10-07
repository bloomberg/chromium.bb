// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_clock_device.h"

#include "base/logging.h"

namespace chromecast {
namespace media {

MediaClockDevice::MediaClockDevice() {
}

MediaClockDevice::~MediaClockDevice() {
}

// static
bool MediaClockDevice::IsValidStateTransition(State state1, State state2) {
  if (state2 == state1)
    return true;

  // All states can transition to |kStateError|.
  bool is_transition_valid = (state2 == kStateError);

  // All the other valid FSM transitions.
  is_transition_valid = is_transition_valid ||
      (state1 == kStateUninitialized && (state2 == kStateIdle)) ||
      (state1 == kStateIdle && (state2 == kStateRunning ||
                                state2 == kStateUninitialized)) ||
      (state1 == kStateRunning && (state2 == kStateIdle)) ||
      (state1 == kStateError && (state2 == kStateUninitialized));

  return is_transition_valid;
}

// static
std::string MediaClockDevice::StateToString(const State& state) {
  switch (state) {
    case kStateUninitialized:
      return "Uninitialized";
    case kStateIdle:
      return "Idle";
    case kStateRunning:
      return "Running";
    case kStateError:
      return "Error";
    default:
      NOTREACHED() << "Unknown MediaClockDevice::State: " << state;
      return "";
  }
}

}  // namespace media
}  // namespace chromecast
