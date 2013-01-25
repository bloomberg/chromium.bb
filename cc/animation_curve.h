// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_CURVE_H_
#define CC_ANIMATION_CURVE_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "ui/gfx/transform.h"

namespace cc {

class FloatAnimationCurve;
class TransformAnimationCurve;
class TransformOperations;

// An animation curve is a function that returns a value given a time.
// There are currently only two types of curve, float and transform.
class CC_EXPORT AnimationCurve {
public:
    enum Type { Float, Transform };

    virtual ~AnimationCurve() { }

    virtual double duration() const = 0;
    virtual Type type() const = 0;
    virtual scoped_ptr<AnimationCurve> clone() const = 0;

    const FloatAnimationCurve* toFloatAnimationCurve() const;
    const TransformAnimationCurve* toTransformAnimationCurve() const;
};

class CC_EXPORT FloatAnimationCurve : public AnimationCurve {
public:
    virtual ~FloatAnimationCurve() { }

    virtual float getValue(double t) const = 0;

    // Partial Animation implementation.
    virtual Type type() const OVERRIDE;
};

class CC_EXPORT TransformAnimationCurve : public AnimationCurve {
public:
    virtual ~TransformAnimationCurve() { }

    virtual gfx::Transform getValue(double t) const = 0;

    // Partial Animation implementation.
    virtual Type type() const OVERRIDE;
};

} // namespace cc

#endif  // CC_ANIMATION_CURVE_H_
