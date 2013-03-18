// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_CURVE_H_
#define CC_ANIMATION_ANIMATION_CURVE_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/transform.h"

namespace cc {

class FloatAnimationCurve;
class TransformAnimationCurve;
class TransformOperations;

// An animation curve is a function that returns a value given a time.
// There are currently only two types of curve, float and transform.
class CC_EXPORT AnimationCurve {
 public:
  enum CurveType { Float, Transform };

  virtual ~AnimationCurve() {}

  virtual double Duration() const = 0;
  virtual CurveType Type() const = 0;
  virtual scoped_ptr<AnimationCurve> Clone() const = 0;

  const FloatAnimationCurve* ToFloatAnimationCurve() const;
  const TransformAnimationCurve* ToTransformAnimationCurve() const;
};

class CC_EXPORT FloatAnimationCurve : public AnimationCurve {
 public:
  virtual ~FloatAnimationCurve() {}

  virtual float GetValue(double t) const = 0;

  // Partial Animation implementation.
  virtual CurveType Type() const OVERRIDE;
};

class CC_EXPORT TransformAnimationCurve : public AnimationCurve {
 public:
  virtual ~TransformAnimationCurve() {}

  virtual gfx::Transform GetValue(double t) const = 0;

  // Partial Animation implementation.
  virtual CurveType Type() const OVERRIDE;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_CURVE_H_
