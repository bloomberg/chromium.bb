// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTimingFunction_h
#define CCTimingFunction_h

#include "UnitBezier.h"
#include "cc/animation_curve.h"

namespace cc {

// See http://www.w3.org/TR/css3-transitions/.
class TimingFunction : public FloatAnimationCurve {
public:
    virtual ~TimingFunction();

    // Partial implementation of FloatAnimationCurve.
    virtual double duration() const OVERRIDE;

protected:
    TimingFunction();
};

class CubicBezierTimingFunction : public TimingFunction {
public:
    static scoped_ptr<CubicBezierTimingFunction> create(double x1, double y1, double x2, double y2);
    virtual ~CubicBezierTimingFunction();

    // Partial implementation of FloatAnimationCurve.
    virtual float getValue(double time) const OVERRIDE;
    virtual scoped_ptr<AnimationCurve> clone() const OVERRIDE;

protected:
    CubicBezierTimingFunction(double x1, double y1, double x2, double y2);

    UnitBezier m_curve;
};

class EaseTimingFunction {
public:
    static scoped_ptr<TimingFunction> create();
};

class EaseInTimingFunction {
public:
    static scoped_ptr<TimingFunction> create();
};

class EaseOutTimingFunction {
public:
    static scoped_ptr<TimingFunction> create();
};

class EaseInOutTimingFunction {
public:
    static scoped_ptr<TimingFunction> create();
};

} // namespace cc

#endif // CCTimingFunction_h
