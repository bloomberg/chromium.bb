// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCAnimationCurve_h
#define CCAnimationCurve_h

#include <public/WebTransformationMatrix.h>
#include <wtf/PassOwnPtr.h>

namespace cc {

class CCFloatAnimationCurve;
class CCTransformAnimationCurve;
class IntSize;
class TransformOperations;

// An animation curve is a function that returns a value given a time.
// There are currently only two types of curve, float and transform.
class CCAnimationCurve {
public:
    enum Type { Float, Transform };

    virtual ~CCAnimationCurve() { }

    virtual double duration() const = 0;
    virtual Type type() const = 0;
    virtual PassOwnPtr<CCAnimationCurve> clone() const = 0;

    const CCFloatAnimationCurve* toFloatAnimationCurve() const;
    const CCTransformAnimationCurve* toTransformAnimationCurve() const;
};

class CCFloatAnimationCurve : public CCAnimationCurve {
public:
    virtual ~CCFloatAnimationCurve() { }

    virtual float getValue(double t) const = 0;

    // Partial CCAnimation implementation.
    virtual Type type() const OVERRIDE { return Float; }
};

class CCTransformAnimationCurve : public CCAnimationCurve {
public:
    virtual ~CCTransformAnimationCurve() { }

    virtual WebKit::WebTransformationMatrix getValue(double t) const = 0;

    // Partial CCAnimation implementation.
    virtual Type type() const OVERRIDE { return Transform; }
};

} // namespace cc

#endif // CCAnimation_h
