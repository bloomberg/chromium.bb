// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <base/scoped_ptr.h>
#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"

#ifndef GESTURES_ACCEL_FILTER_INTERPRETER_H_
#define GESTURES_ACCEL_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter provides pointer and scroll acceleration based on
// an acceleration curve and the user's sensitivity setting.

class AccelFilterInterpreter : public Interpreter {
  FRIEND_TEST(AccelFilterInterpreterTest, SimpleTest);
  FRIEND_TEST(AccelFilterInterpreterTest, TimingTest);
  FRIEND_TEST(AccelFilterInterpreterTest, CustomAccelTest);
 public:
  // Takes ownership of |next|:
  explicit AccelFilterInterpreter(Interpreter* next);
  virtual ~AccelFilterInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

  virtual void Configure(GesturesPropProvider* pp, void* data);

  virtual void Deconfigure(GesturesPropProvider* pp, void* data);

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

  scoped_ptr<Interpreter> next_;

  static const size_t kMaxCurveSegs = 2;
  static const size_t kMaxCustomCurveSegs = 20;
  static const size_t kMaxAccelCurves = 5;

  // curves for sensitivity 1..5
  CurveSegment curves_[kMaxAccelCurves][kMaxCurveSegs];

  // Custom curves
  CurveSegment custom_point_[kMaxCustomCurveSegs];
  CurveSegment custom_scroll_[kMaxCustomCurveSegs];

  int sensitivity_;  // [1..5]
  GesturesProp* sensitivity_prop_;

  static const size_t kMaxCurveSegStrLen = 30;
  static const size_t kCacheStrLen = kMaxCustomCurveSegs * kMaxCurveSegStrLen;

  const char* custom_point_str_;
  char last_parsed_custom_point_str_[kCacheStrLen];
  GesturesProp* custom_point_str_prop_;
  const char* custom_scroll_str_;
  char last_parsed_custom_scroll_str_[kCacheStrLen];
  GesturesProp* custom_scroll_str_prop_;
};

}  // namespace gestures

#endif  // GESTURES_SCALING_FILTER_INTERPRETER_H_
