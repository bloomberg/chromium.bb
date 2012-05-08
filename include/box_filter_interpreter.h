// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/finger_metrics.h"
#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/map.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_BOX_FILTER_INTERPRETER_H_
#define GESTURES_BOX_FILTER_INTERPRETER_H_

namespace gestures {

// This filter interpreter applies simple "box" algorithm to fingers as they
// pass through the filter. The purpose is to filter noisy input.
// The algorithm is: For each axis:
// - For each input point, compare distance to previous output point for
//   the finger.
// - If the distance is under box_width_ / 2, report the old location.
// - Report the new point shifted box_width_ / 2 toward the previous
//   output point.
//
// The way to think about this is that there is a box around the previous
// output point with a width and height of box_width_. If a new point is
// within that box, report the old location (don't move the box). If the new
// point is outside, shift the box over to include the new point, then report
// the new center of the box.

class BoxFilterInterpreter : public Interpreter, public PropertyDelegate {
  FRIEND_TEST(BoxFilterInterpreterTest, SimpleTest);
 public:
  // Takes ownership of |next|:
  BoxFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~BoxFilterInterpreter() {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

 private:
  scoped_ptr<Interpreter> next_;

  DoubleProperty box_width_;

  map<short, FingerState, kMaxFingers> previous_output_;
};

}  // namespace gestures

#endif  // GESTURES_BOX_FILTER_INTERPRETER_H_
