// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTimingFunction_h
#define CCTimingFunction_h

#include "CCAnimationCurve.h"
#include "UnitBezier.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

// See http://www.w3.org/TR/css3-transitions/.
class CCTimingFunction : public CCFloatAnimationCurve {
public:
    virtual ~CCTimingFunction();

    // Partial implementation of CCFloatAnimationCurve.
    virtual double duration() const OVERRIDE;

protected:
    CCTimingFunction();
};

class CCCubicBezierTimingFunction : public CCTimingFunction {
public:
    static PassOwnPtr<CCCubicBezierTimingFunction> create(double x1, double y1, double x2, double y2);
    virtual ~CCCubicBezierTimingFunction();

    // Partial implementation of CCFloatAnimationCurve.
    virtual float getValue(double time) const OVERRIDE;
    virtual PassOwnPtr<CCAnimationCurve> clone() const OVERRIDE;

protected:
    CCCubicBezierTimingFunction(double x1, double y1, double x2, double y2);

    UnitBezier m_curve;
};

class CCEaseTimingFunction {
public:
    static PassOwnPtr<CCTimingFunction> create();
};

class CCEaseInTimingFunction {
public:
    static PassOwnPtr<CCTimingFunction> create();
};

class CCEaseOutTimingFunction {
public:
    static PassOwnPtr<CCTimingFunction> create();
};

class CCEaseInOutTimingFunction {
public:
    static PassOwnPtr<CCTimingFunction> create();
};

} // namespace cc

#endif // CCTimingFunction_h
