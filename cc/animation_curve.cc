// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation_curve.h"

#include "base/logging.h"

namespace cc {

const FloatAnimationCurve* AnimationCurve::ToFloatAnimationCurve() const {
  DCHECK(Type() == AnimationCurve::Float);
  return static_cast<const FloatAnimationCurve*>(this);
}

AnimationCurve::CurveType FloatAnimationCurve::Type() const {
  return Float;
}

const TransformAnimationCurve* AnimationCurve::ToTransformAnimationCurve()
    const {
  DCHECK(Type() == AnimationCurve::Transform);
  return static_cast<const TransformAnimationCurve*>(this);
}

AnimationCurve::CurveType TransformAnimationCurve::Type() const {
  return Transform;
}

}  // namespace cc
