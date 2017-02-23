// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_EASING_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_EASING_H_

#include "base/macros.h"
#include "ui/gfx/geometry/cubic_bezier.h"

namespace vr_shell {
namespace easing {

enum EasingType {
  LINEAR = 0,
  CUBICBEZIER,
  EASEIN,
  EASEOUT,
  EASEINOUT,
};

// Abstract base class for custom interpolators, mapping linear input between
// 0 and 1 to custom values between those two points.
class Easing {
 public:
  // Compute an output value, given an input between 0 and 1. Output will
  // equal input at (at least) points 0 and 1.
  double CalculateValue(double input);
  virtual ~Easing() {}

 protected:
  Easing() {}
  virtual double CalculateValueImpl(double input) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Easing);
};

// Linear interpolation generates output equal to input.
class Linear : public Easing {
 public:
  Linear() = default;
  double CalculateValueImpl(double input) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Linear);
};

// Computes a cubic-bezier transition based on two control points.
class CubicBezier : public Easing {
 public:
  CubicBezier(double p1x, double p1y, double p2x, double p2y);
  double CalculateValueImpl(double input) override;

 private:
  gfx::CubicBezier bezier_;
  DISALLOW_COPY_AND_ASSIGN(CubicBezier);
};

// Computes |input|^|power|.
class EaseIn : public Easing {
 public:
  explicit EaseIn(double power);
  double CalculateValueImpl(double input) override;

 private:
  double power_;
  DISALLOW_COPY_AND_ASSIGN(EaseIn);
};

// Computes 1 - |input|^|power|.
class EaseOut : public Easing {
 public:
  explicit EaseOut(double power);
  double CalculateValueImpl(double input) override;

 private:
  double power_;
  DISALLOW_COPY_AND_ASSIGN(EaseOut);
};

// Starts with EaseIn and finishes with EaseOut.
class EaseInOut : public Easing {
 public:
  explicit EaseInOut(double power);
  double CalculateValueImpl(double input) override;

 private:
  EaseIn ease_in_;
  DISALLOW_COPY_AND_ASSIGN(EaseInOut);
};

}  // namespace easing
}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_EASING_H_
