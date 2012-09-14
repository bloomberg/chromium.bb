// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTimingFunction.h"

#include <wtf/OwnPtr.h>

namespace {
const double epsilon = 1e-6;
} // namespace

namespace cc {

CCTimingFunction::CCTimingFunction()
{
}

CCTimingFunction::~CCTimingFunction()
{
}

double CCTimingFunction::duration() const
{
    return 1.0;
}

PassOwnPtr<CCCubicBezierTimingFunction> CCCubicBezierTimingFunction::create(double x1, double y1, double x2, double y2)
{
    return adoptPtr(new CCCubicBezierTimingFunction(x1, y1, x2, y2));
}

CCCubicBezierTimingFunction::CCCubicBezierTimingFunction(double x1, double y1, double x2, double y2)
    : m_curve(x1, y1, x2, y2)
{
}

CCCubicBezierTimingFunction::~CCCubicBezierTimingFunction()
{
}

float CCCubicBezierTimingFunction::getValue(double x) const
{
    UnitBezier temp(m_curve);
    return static_cast<float>(temp.solve(x, epsilon));
}

PassOwnPtr<CCAnimationCurve> CCCubicBezierTimingFunction::clone() const
{
    return adoptPtr(new CCCubicBezierTimingFunction(*this));
}

// These numbers come from http://www.w3.org/TR/css3-transitions/#transition-timing-function_tag.
PassOwnPtr<CCTimingFunction> CCEaseTimingFunction::create()
{
    return CCCubicBezierTimingFunction::create(0.25, 0.1, 0.25, 1);
}

PassOwnPtr<CCTimingFunction> CCEaseInTimingFunction::create()
{
    return CCCubicBezierTimingFunction::create(0.42, 0, 1.0, 1);
}

PassOwnPtr<CCTimingFunction> CCEaseOutTimingFunction::create()
{
    return CCCubicBezierTimingFunction::create(0, 0, 0.58, 1);
}

PassOwnPtr<CCTimingFunction> CCEaseInOutTimingFunction::create()
{
    return CCCubicBezierTimingFunction::create(0.42, 0, 0.58, 1);
}

} // namespace cc
