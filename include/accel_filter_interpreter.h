// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/filter_interpreter.h"
#include "gestures/include/gestures.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/tracer.h"

#ifndef GESTURES_ACCEL_FILTER_INTERPRETER_H_
#define GESTURES_ACCEL_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter provides pointer and scroll acceleration based on
// an acceleration curve and the user's sensitivity setting.

class AccelFilterInterpreter : public FilterInterpreter {
  FRIEND_TEST(AccelFilterInterpreterTest, CustomAccelTest);
  FRIEND_TEST(AccelFilterInterpreterTest, SimpleTest);
  FRIEND_TEST(AccelFilterInterpreterTest, TimingTest);
  FRIEND_TEST(AccelFilterInterpreterTest, TinyMoveTest);
 public:
  // Takes ownership of |next|:
  AccelFilterInterpreter(PropRegistry* prop_reg, Interpreter* next,
                         Tracer* tracer);
  virtual ~AccelFilterInterpreter() {}

 protected:
  virtual Gesture* SyncInterpretImpl(HardwareState* hwstate,
                                     stime_t* timeout);

  virtual Gesture* HandleTimerImpl(stime_t now, stime_t* timeout);

 private:
  struct CurveSegment {
    CurveSegment() : x_(INFINITY), sqr_(0.0), mul_(1.0), int_(0.0) {}
    CurveSegment(float x, float s, float m, float b)
        : x_(x), sqr_(s), mul_(m), int_(b) {}
    CurveSegment(const CurveSegment& that)
        : x_(that.x_), sqr_(that.sqr_), mul_(that.mul_), int_(that.int_) {}
    float x_;  // Max X value of segment. User's point will be less than this.
    float sqr_;  // x^2 multiplier
    float mul_;  // Slope of line (x multiplier)
    float int_;  // Intercept of line
  };

  void ParseCurveString(const char* input, char* cache, CurveSegment* out_segs);

  void ScaleGesture(Gesture* gs);

  static const size_t kMaxCurveSegs = 3;
  static const size_t kMaxCustomCurveSegs = 20;
  static const size_t kMaxAccelCurves = 5;

  // curves for sensitivity 1..5
  CurveSegment point_curves_[kMaxAccelCurves][kMaxCurveSegs];
  CurveSegment mouse_point_curves_[kMaxAccelCurves][kMaxCurveSegs];
  CurveSegment scroll_curves_[kMaxAccelCurves][kMaxCurveSegs];

  // Custom curves
  CurveSegment custom_point_[kMaxCustomCurveSegs];
  CurveSegment custom_scroll_[kMaxCustomCurveSegs];

  IntProperty pointer_sensitivity_;  // [1..5] or 0 for custom
  IntProperty scroll_sensitivity_;  // [1..5] or 0 for custom

  static const size_t kMaxCurveSegStrLen = 30;
  static const size_t kCacheStrLen = kMaxCustomCurveSegs * kMaxCurveSegStrLen;

  StringProperty custom_point_str_;
  char last_parsed_custom_point_str_[kCacheStrLen];
  StringProperty custom_scroll_str_;
  char last_parsed_custom_scroll_str_[kCacheStrLen];

  DoubleProperty point_x_out_scale_;
  DoubleProperty point_y_out_scale_;
  DoubleProperty scroll_x_out_scale_;
  DoubleProperty scroll_y_out_scale_;
  BoolProperty use_mouse_point_curves_;
};

}  // namespace gestures

#endif  // GESTURES_SCALING_FILTER_INTERPRETER_H_
