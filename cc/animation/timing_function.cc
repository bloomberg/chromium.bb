// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "cc/animation/timing_function.h"

namespace cc {

namespace {

static const double BEZIER_EPSILON = 1e-7;
static const int MAX_STEPS = 30;

static double eval_bezier(double x1, double x2, double t) {
  const double x1_times_3 = 3.0 * x1;
  const double x2_times_3 = 3.0 * x2;
  const double h3 = x1_times_3;
  const double h1 = x1_times_3 - x2_times_3 + 1.0;
  const double h2 = x2_times_3 - 6.0 * x1;
  return t * (t * (t * h1 + h2) + h3);
}

static double bezier_interp(double x1,
                            double y1,
                            double x2,
                            double y2,
                            double x) {
  DCHECK_GE(1.0, x1);
  DCHECK_LE(0.0, x1);
  DCHECK_GE(1.0, x2);
  DCHECK_LE(0.0, x2);

  x1 = std::min(std::max(x1, 0.0), 1.0);
  x2 = std::min(std::max(x2, 0.0), 1.0);
  x = std::min(std::max(x, 0.0), 1.0);

  // Step 1. Find the t corresponding to the given x. I.e., we want t such that
  // eval_bezier(x1, x2, t) = x. There is a unique solution if x1 and x2 lie
  // within (0, 1).
  //
  // We're just going to do bisection for now (for simplicity), but we could
  // easily do some newton steps if this turns out to be a bottleneck.
  double t = 0.0;
  double step = 1.0;
  for (int i = 0; i < MAX_STEPS; ++i, step *= 0.5) {
    const double error = eval_bezier(x1, x2, t) - x;
    if (std::abs(error) < BEZIER_EPSILON)
      break;
    t += error > 0.0 ? -step : step;
  }

  // We should have terminated the above loop because we got close to x, not
  // because we exceeded MAX_STEPS. Do a DCHECK here to confirm.
  DCHECK_GT(BEZIER_EPSILON, std::abs(eval_bezier(x1, x2, t) - x));

  // Step 2. Return the interpolated y values at the t we computed above.
  return eval_bezier(y1, y2, t);
}

}  // namespace

TimingFunction::TimingFunction() {}

TimingFunction::~TimingFunction() {}

double TimingFunction::Duration() const {
  return 1.0;
}

scoped_ptr<CubicBezierTimingFunction> CubicBezierTimingFunction::Create(
    double x1, double y1, double x2, double y2) {
  return make_scoped_ptr(new CubicBezierTimingFunction(x1, y1, x2, y2));
}

CubicBezierTimingFunction::CubicBezierTimingFunction(double x1,
                                                     double y1,
                                                     double x2,
                                                     double y2)
    : x1_(x1), y1_(y1), x2_(x2), y2_(y2) {}

CubicBezierTimingFunction::~CubicBezierTimingFunction() {}

float CubicBezierTimingFunction::GetValue(double x) const {
  return static_cast<float>(bezier_interp(x1_, y1_, x2_, y2_, x));
}

scoped_ptr<AnimationCurve> CubicBezierTimingFunction::Clone() const {
  return make_scoped_ptr(
      new CubicBezierTimingFunction(*this)).PassAs<AnimationCurve>();
}

// These numbers come from
// http://www.w3.org/TR/css3-transitions/#transition-timing-function_tag.
scoped_ptr<TimingFunction> EaseTimingFunction::Create() {
  return CubicBezierTimingFunction::Create(
      0.25, 0.1, 0.25, 1.0).PassAs<TimingFunction>();
}

scoped_ptr<TimingFunction> EaseInTimingFunction::Create() {
  return CubicBezierTimingFunction::Create(
      0.42, 0.0, 1.0, 1.0).PassAs<TimingFunction>();
}

scoped_ptr<TimingFunction> EaseOutTimingFunction::Create() {
  return CubicBezierTimingFunction::Create(
      0.0, 0.0, 0.58, 1.0).PassAs<TimingFunction>();
}

scoped_ptr<TimingFunction> EaseInOutTimingFunction::Create() {
  return CubicBezierTimingFunction::Create(
      0.42, 0.0, 0.58, 1).PassAs<TimingFunction>();
}

}  // namespace cc
