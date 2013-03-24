// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/timing_function.h"

#include "third_party/skia/include/core/SkMath.h"

// TODO(danakj) These methods come from SkInterpolator.cpp. When such a method
// is available in the public Skia API, we should switch to using that.
// http://crbug.com/159735
namespace {

// Dot14 has 14 bits for decimal places, and the remainder for whole numbers.
typedef int Dot14;
#define DOT14_ONE (1 << 14)
#define DOT14_HALF (1 << 13)

static inline Dot14 Dot14Mul(Dot14 a, Dot14 b) {
  return (a * b + DOT14_HALF) >> 14;
}

static inline Dot14 EvalCubic(Dot14 t, Dot14 A, Dot14 B, Dot14 C) {
  return Dot14Mul(Dot14Mul(Dot14Mul(C, t) + B, t) + A, t);
}

static inline Dot14 PinAndConvert(SkScalar x) {
  if (x <= 0)
    return 0;
  if (x >= SK_Scalar1)
    return DOT14_ONE;
  return SkScalarToFixed(x) >> 2;
}

SkScalar SkUnitCubicInterp(SkScalar bx,
                           SkScalar by,
                           SkScalar cx,
                           SkScalar cy,
                           SkScalar value) {
  Dot14 x = PinAndConvert(value);

  if (x == 0)
    return 0;
  if (x == DOT14_ONE)
    return SK_Scalar1;

  Dot14 b = PinAndConvert(bx);
  Dot14 c = PinAndConvert(cx);

  // Now compute our coefficients from the control points.
  //  t   -> 3b
  //  t^2 -> 3c - 6b
  //  t^3 -> 3b - 3c + 1
  Dot14 A = 3 * b;
  Dot14 B = 3 * (c - 2 * b);
  Dot14 C = 3 * (b - c) + DOT14_ONE;

  // Now search for a t value given x.
  Dot14 t = DOT14_HALF;
  Dot14 dt = DOT14_HALF;
  for (int i = 0; i < 13; i++) {
    dt >>= 1;
    Dot14 guess = EvalCubic(t, A, B, C);
    if (x < guess)
      t -= dt;
    else
      t += dt;
  }

  // Now we have t, so compute the coefficient for Y and evaluate.
  b = PinAndConvert(by);
  c = PinAndConvert(cy);
  A = 3 * b;
  B = 3 * (c - 2 * b);
  C = 3 * (b - c) + DOT14_ONE;
  return SkFixedToScalar(EvalCubic(t, A, B, C) << 2);
}

}  // namespace

namespace cc {

TimingFunction::TimingFunction() {}

TimingFunction::~TimingFunction() {}

double TimingFunction::Duration() const {
  return 1.0;
}

scoped_ptr<CubicBezierTimingFunction> CubicBezierTimingFunction::Create(
    double x1,
    double y1,
    double x2,
    double y2) {
  return make_scoped_ptr(new CubicBezierTimingFunction(x1, y1, x2, y2));
}

CubicBezierTimingFunction::CubicBezierTimingFunction(double x1,
                                                     double y1,
                                                     double x2,
                                                     double y2)
    : x1_(SkDoubleToScalar(x1)),
      y1_(SkDoubleToScalar(y1)),
      x2_(SkDoubleToScalar(x2)),
      y2_(SkDoubleToScalar(y2)) {}

CubicBezierTimingFunction::~CubicBezierTimingFunction() {}

float CubicBezierTimingFunction::GetValue(double x) const {
  SkScalar value = SkUnitCubicInterp(x1_, y1_, x2_, y2_, x);
  return SkScalarToFloat(value);
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
