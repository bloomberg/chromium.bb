// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/touch_fling_gesture_curve.h"

#include <cmath>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebGestureCurveTarget.h"
#include "third_party/WebKit/public/platform/WebSize.h"

using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureCurve;
using blink::WebGestureCurveTarget;
using blink::WebSize;

namespace {

const char* kCurveName = "TouchFlingGestureCurve";

// The touchpad / touchscreen fling profiles are a matched set
// determined via UX experimentation. Do not modify without
// first discussing with rjkroege@chromium.org or
// wjmaclean@chromium.org.
const float kDefaultAlpha = -5.70762e+03f;
const float kDefaultBeta = 1.72e+02f;
const float kDefaultGamma = 3.7e+00f;

inline double position(double t) {
  return kDefaultAlpha * exp(-kDefaultGamma * t) - kDefaultBeta * t -
         kDefaultAlpha;
}

inline double velocity(double t) {
  return -kDefaultAlpha * kDefaultGamma * exp(-kDefaultGamma * t) -
         kDefaultBeta;
}

inline double timeAtVelocity(double v) {
  return -log((v + kDefaultBeta) / (-kDefaultAlpha * kDefaultGamma)) /
         kDefaultGamma;
}

} // namespace


namespace content {

// This curve implementation is based on the notion of a single, absolute
// curve, which starts at a large velocity and smoothly decreases to
// zero. For a given input velocity, we find where on the curve this
// velocity occurs, and start the animation at this point---denoted by
// (time_offset_, position_offset_).
//
// This has the effect of automatically determining an animation duration
// that scales with input velocity, as faster initial velocities start
// earlier on the curve and thus take longer to reach the end. No
// complicated time scaling is required.
//
// Since the starting velocity is implicitly determined by our starting
// point, we only store the relative magnitude and direction of both
// initial x- and y-velocities, and use this to scale the computed
// displacement at any point in time. This guarantees that fling
// trajectories are straight lines when viewed in x-y space. Initial
// velocities that lie outside the max velocity are constrained to start
// at zero (and thus are implicitly scaled).
//
// The curve is modelled as a 4th order polynomial, starting at t = 0,
// and ending at t = curve_duration_. Attempts to generate
// position/velocity estimates outside this range are undefined.

WebGestureCurve* TouchFlingGestureCurve::Create(
    const WebFloatPoint& initial_velocity,
    const WebSize& cumulative_scroll) {
  return new TouchFlingGestureCurve(initial_velocity, cumulative_scroll);
}

TouchFlingGestureCurve::TouchFlingGestureCurve(
    const WebFloatPoint& initial_velocity,
    const WebSize& cumulative_scroll)
    : cumulative_scroll_(WebFloatSize(cumulative_scroll.width,
                                      cumulative_scroll.height)) {
  DCHECK(initial_velocity != WebFloatPoint());

  // Curve ends when velocity reaches zero.
  curve_duration_ = timeAtVelocity(0);
  DCHECK(curve_duration_ > 0);

  float max_start_velocity = std::max(fabs(initial_velocity.x),
                                      fabs(initial_velocity.y));

  // Force max_start_velocity to lie in the range v(0) to v(curve_duration),
  // and assume that the curve parameters define a monotonically decreasing
  // velocity, or else bisection search may fail.
  if (max_start_velocity > velocity(0))
    max_start_velocity = velocity(0);

  if (max_start_velocity < 0)
    max_start_velocity = 0;

  // We keep track of relative magnitudes and directions of the
  // velocity/displacement components here.
  displacement_ratio_ = WebFloatPoint(initial_velocity.x / max_start_velocity,
                                      initial_velocity.y / max_start_velocity);

  // Compute time-offset for start velocity.
  time_offset_ = timeAtVelocity(max_start_velocity);

  // Compute curve position at offset time
  position_offset_ = position(time_offset_);
  TRACE_EVENT_ASYNC_BEGIN1("input", "GestureAnimation", this, "curve",
      kCurveName);
}

TouchFlingGestureCurve::~TouchFlingGestureCurve() {
  TRACE_EVENT_ASYNC_END0("input", "GestureAnimation", this);
}

bool TouchFlingGestureCurve::apply(double time, WebGestureCurveTarget* target) {
  // If the fling has yet to start, simply return and report true to prevent
  // fling termination.
  if (time <= 0)
    return true;

  float displacement;
  float speed;
  if (time + time_offset_ < curve_duration_) {
    displacement = position(time + time_offset_) - position_offset_;
    speed = velocity(time + time_offset_);
  } else {
    displacement = position(curve_duration_) - position_offset_;
    speed = 0.f;
  }

  // Keep track of integer portion of scroll thus far, and prepare increment.
  WebFloatSize scroll(displacement * displacement_ratio_.x,
                      displacement * displacement_ratio_.y);
  WebFloatSize scroll_increment(scroll.width - cumulative_scroll_.width,
                                scroll.height - cumulative_scroll_.height);
  WebFloatSize scroll_velocity(speed * displacement_ratio_.x,
                               speed * displacement_ratio_.y);
  cumulative_scroll_ = scroll;

  if (time + time_offset_ < curve_duration_ ||
      scroll_increment != WebFloatSize()) {
    // scrollBy() could delete this curve if the animation is over, so don't
    // touch any member variables after making that call.
    return target->scrollBy(scroll_increment, scroll_velocity);
  }

  return false;
}

} // namespace content
