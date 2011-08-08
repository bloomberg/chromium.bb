// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"

#ifndef GESTURES_INTEGRAL_GESTURE_FILTER_INTERPRETER_H_
#define GESTURES_INTEGRAL_GESTURE_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter passes HardwareState unmodified to next_. All gestures
// that pass through, though, are changed to have integral values. Any
// remainder is stored and added to the next gestures. This means that if
// a user is very slowly rolling their finger, many gestures w/ values < 1
// can be accumulated and together create a move of a single pixel.

class IntegralGestureFilterInterpreter : public Interpreter {
 public:
  // Takes ownership of |next|:
  explicit IntegralGestureFilterInterpreter(Interpreter* next);
  virtual ~IntegralGestureFilterInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

  virtual void Configure(GesturesPropProvider* pp, void* data);

  virtual void Deconfigure(GesturesPropProvider* pp, void* data);

 private:
  void HandleGesture(Gesture* gs);

  scoped_ptr<Interpreter> next_;

  float x_move_remainder_, y_move_remainder_;
  float hscroll_remainder_, vscroll_remainder_;
};

}  // namespace gestures

#endif  // GESTURES_INTEGRAL_GESTURE_FILTER_INTERPRETER_H_
