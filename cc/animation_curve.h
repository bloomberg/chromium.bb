// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCAnimationCurve_h
#define CCAnimationCurve_h

#include "base/memory/scoped_ptr.h"
#include <public/WebTransformationMatrix.h>

namespace cc {

class FloatAnimationCurve;
class TransformAnimationCurve;
class IntSize;
class TransformOperations;

// An animation curve is a function that returns a value given a time.
// There are currently only two types of curve, float and transform.
class AnimationCurve {
public:
    enum Type { Float, Transform };

    virtual ~AnimationCurve() { }

    virtual double duration() const = 0;
    virtual Type type() const = 0;
    virtual scoped_ptr<AnimationCurve> clone() const = 0;

    const FloatAnimationCurve* toFloatAnimationCurve() const;
    const TransformAnimationCurve* toTransformAnimationCurve() const;
};

class FloatAnimationCurve : public AnimationCurve {
public:
    virtual ~FloatAnimationCurve() { }

    virtual float getValue(double t) const = 0;

    // Partial Animation implementation.
    virtual Type type() const OVERRIDE;
};

class TransformAnimationCurve : public AnimationCurve {
public:
    virtual ~TransformAnimationCurve() { }

    virtual WebKit::WebTransformationMatrix getValue(double t) const = 0;

    // Partial Animation implementation.
    virtual Type type() const OVERRIDE;
};

} // namespace cc

#endif // CCAnimation_h
