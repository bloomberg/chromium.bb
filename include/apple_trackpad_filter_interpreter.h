// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // For FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_APPLE_TRACKPAD_FILTER_INTERPRETER_H_
#define GESTURES_APPLE_TRACKPAD_FILTER_INTERPRETER_H_

namespace gestures {

// This filter interpreter approximates Apple Magic Trackpad's pressue
// value using touch_major value so that it can work with gestures library.

class AppleTrackpadFilterInterpreter : public Interpreter,
                                    public PropertyDelegate {
  FRIEND_TEST(AppleTrackpadFilterInterpreterTest, SimpleTest);
 public:
  // Takes ownership of |next|:
  AppleTrackpadFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~AppleTrackpadFilterInterpreter() {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

 private:
  scoped_ptr<Interpreter> next_;
  // Whether or not this filter is enabled. If disabled, it behaves as a
  // simple passthrough.
  BoolProperty enabled_;
};

}  // namespace gestures

#endif  // GESTURES_APPLE_TRACKPAD_FILTER_INTERPRETER_H_
