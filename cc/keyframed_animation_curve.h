// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCKeyframedAnimationCurve_h
#define CCKeyframedAnimationCurve_h

#include "cc/animation_curve.h"
#include "cc/timing_function.h"
#include "scoped_ptr_vector.h"
#include <public/WebTransformOperations.h>

namespace cc {

class CCKeyframe {
public:
    double time() const;
    const CCTimingFunction* timingFunction() const;

protected:
    CCKeyframe(double time, scoped_ptr<CCTimingFunction>);
    virtual ~CCKeyframe();

private:
    double m_time;
    scoped_ptr<CCTimingFunction> m_timingFunction;
};

class CCFloatKeyframe : public CCKeyframe {
public:
    static scoped_ptr<CCFloatKeyframe> create(double time, float value, scoped_ptr<CCTimingFunction>);
    virtual ~CCFloatKeyframe();

    float value() const;

    scoped_ptr<CCFloatKeyframe> clone() const;

private:
    CCFloatKeyframe(double time, float value, scoped_ptr<CCTimingFunction>);

    float m_value;
};

class CCTransformKeyframe : public CCKeyframe {
public:
    static scoped_ptr<CCTransformKeyframe> create(double time, const WebKit::WebTransformOperations& value, scoped_ptr<CCTimingFunction>);
    virtual ~CCTransformKeyframe();

    const WebKit::WebTransformOperations& value() const;

    scoped_ptr<CCTransformKeyframe> clone() const;

private:
    CCTransformKeyframe(double time, const WebKit::WebTransformOperations& value, scoped_ptr<CCTimingFunction>);

    WebKit::WebTransformOperations m_value;
};

class CCKeyframedFloatAnimationCurve : public CCFloatAnimationCurve {
public:
    // It is required that the keyframes be sorted by time.
    static scoped_ptr<CCKeyframedFloatAnimationCurve> create();

    virtual ~CCKeyframedFloatAnimationCurve();

    void addKeyframe(scoped_ptr<CCFloatKeyframe>);

    // CCAnimationCurve implementation
    virtual double duration() const OVERRIDE;
    virtual scoped_ptr<CCAnimationCurve> clone() const OVERRIDE;

    // CCFloatAnimationCurve implementation
    virtual float getValue(double t) const OVERRIDE;

private:
    CCKeyframedFloatAnimationCurve();

    // Always sorted in order of increasing time. No two keyframes have the
    // same time.
    ScopedPtrVector<CCFloatKeyframe> m_keyframes;
};

class CCKeyframedTransformAnimationCurve : public CCTransformAnimationCurve {
public:
    // It is required that the keyframes be sorted by time.
    static scoped_ptr<CCKeyframedTransformAnimationCurve> create();

    virtual ~CCKeyframedTransformAnimationCurve();

    void addKeyframe(scoped_ptr<CCTransformKeyframe>);

    // CCAnimationCurve implementation
    virtual double duration() const OVERRIDE;
    virtual scoped_ptr<CCAnimationCurve> clone() const OVERRIDE;

    // CCTransformAnimationCurve implementation
    virtual WebKit::WebTransformationMatrix getValue(double t) const OVERRIDE;

private:
    CCKeyframedTransformAnimationCurve();

    // Always sorted in order of increasing time. No two keyframes have the
    // same time.
    ScopedPtrVector<CCTransformKeyframe> m_keyframes;
};

} // namespace cc

#endif // CCKeyframedAnimationCurve_h
