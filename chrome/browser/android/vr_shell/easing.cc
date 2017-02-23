// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/easing.h"

#include <cmath>

#include "base/logging.h"

namespace vr_shell {
namespace easing {

double Easing::CalculateValue(double input) {
  DCHECK(input >= 0.0 && input <= 1.0);
  return CalculateValueImpl(input);
}

CubicBezier::CubicBezier(double p1x, double p1y, double p2x, double p2y)
    : bezier_(p1x, p1y, p2x, p2y) {}

double CubicBezier::CalculateValueImpl(double state) {
  return bezier_.Solve(state);
}

EaseIn::EaseIn(double power) : power_(power) {}
double EaseIn::CalculateValueImpl(double state) {
  return pow(state, power_);
}

EaseOut::EaseOut(double power) : power_(power) {}
double EaseOut::CalculateValueImpl(double state) {
  return 1.0 - pow(1.0 - state, power_);
}

EaseInOut::EaseInOut(double power) : ease_in_(power) {}
double EaseInOut::CalculateValueImpl(double state) {
  if (state < 0.5) {
    return ease_in_.CalculateValueImpl(state * 2) / 2;
  } else {
    return 1.0 - ease_in_.CalculateValueImpl((1.0 - state) * 2) / 2;
  }
}

double Linear::CalculateValueImpl(double state) {
  return state;
}

}  // namespace easing
}  // namespace vr_shell
