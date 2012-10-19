// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTimingFunction.h"

namespace {
const double epsilon = 1e-6;
} // namespace

namespace cc {

TimingFunction::TimingFunction()
{
}

TimingFunction::~TimingFunction()
{
}

double TimingFunction::duration() const
{
    return 1.0;
}

scoped_ptr<CubicBezierTimingFunction> CubicBezierTimingFunction::create(double x1, double y1, double x2, double y2)
{
    return make_scoped_ptr(new CubicBezierTimingFunction(x1, y1, x2, y2));
}

CubicBezierTimingFunction::CubicBezierTimingFunction(double x1, double y1, double x2, double y2)
    : m_curve(x1, y1, x2, y2)
{
}

CubicBezierTimingFunction::~CubicBezierTimingFunction()
{
}

float CubicBezierTimingFunction::getValue(double x) const
{
    UnitBezier temp(m_curve);
    return static_cast<float>(temp.solve(x, epsilon));
}

scoped_ptr<AnimationCurve> CubicBezierTimingFunction::clone() const
{
    return make_scoped_ptr(new CubicBezierTimingFunction(*this)).PassAs<AnimationCurve>();
}

// These numbers come from http://www.w3.org/TR/css3-transitions/#transition-timing-function_tag.
scoped_ptr<TimingFunction> EaseTimingFunction::create()
{
    return CubicBezierTimingFunction::create(0.25, 0.1, 0.25, 1).PassAs<TimingFunction>();
}

scoped_ptr<TimingFunction> EaseInTimingFunction::create()
{
    return CubicBezierTimingFunction::create(0.42, 0, 1.0, 1).PassAs<TimingFunction>();
}

scoped_ptr<TimingFunction> EaseOutTimingFunction::create()
{
    return CubicBezierTimingFunction::create(0, 0, 0.58, 1).PassAs<TimingFunction>();
}

scoped_ptr<TimingFunction> EaseInOutTimingFunction::create()
{
    return CubicBezierTimingFunction::create(0.42, 0, 0.58, 1).PassAs<TimingFunction>();
}

} // namespace cc
