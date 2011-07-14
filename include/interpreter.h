// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_INTERPRETER_H__
#define GESTURES_INTERPRETER_H__

#include "gestures/include/gestures.h"

// This is a collection of supporting structs and an interface for
// Interpreters.

struct HardwareState;

namespace gestures {

// Interface for all interpreters. Interpreters currently are synchronous.
// A synchronous interpreter will return  0 or 1 Gestures for each passed in
// HardwareState.

class Interpreter {
 public:
  virtual ~Interpreter() {}

  // Called to interpret the current state and optionally produce 1
  // resulting gesture. The passed |hwstate| may be modified.
  virtual Gesture* SyncInterpret(HardwareState* hwstate) = 0;

  virtual void SetHardwareProperties(const HardwareProperties& hwprops) {}
};

}  // namespace gestures

#endif  // GESTURES_INTERPRETER_H__
