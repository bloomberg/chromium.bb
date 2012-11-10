// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation_curve.h"

#include "base/logging.h"

namespace cc {

const FloatAnimationCurve* AnimationCurve::toFloatAnimationCurve() const
{
    DCHECK(type() == AnimationCurve::Float);
    return static_cast<const FloatAnimationCurve*>(this);
}

AnimationCurve::Type FloatAnimationCurve::type() const
{
    return Float;
}

const TransformAnimationCurve* AnimationCurve::toTransformAnimationCurve() const
{
    DCHECK(type() == AnimationCurve::Transform);
    return static_cast<const TransformAnimationCurve*>(this);
}

AnimationCurve::Type TransformAnimationCurve::type() const
{
    return Transform;
}

}  // namespace cc
