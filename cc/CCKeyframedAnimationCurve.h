// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCKeyframedAnimationCurve_h
#define CCKeyframedAnimationCurve_h

#include "CCAnimationCurve.h"
#include "CCTimingFunction.h"
#include <public/WebTransformOperations.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace cc {

class CCKeyframe {
public:
    double time() const;
    const CCTimingFunction* timingFunction() const;

protected:
    CCKeyframe(double time, PassOwnPtr<CCTimingFunction>);
    virtual ~CCKeyframe();

private:
    double m_time;
    OwnPtr<CCTimingFunction> m_timingFunction;
};

class CCFloatKeyframe : public CCKeyframe {
public:
    static PassOwnPtr<CCFloatKeyframe> create(double time, float value, PassOwnPtr<CCTimingFunction>);
    virtual ~CCFloatKeyframe();

    float value() const;

    PassOwnPtr<CCFloatKeyframe> clone() const;

private:
    CCFloatKeyframe(double time, float value, PassOwnPtr<CCTimingFunction>);

    float m_value;
};

class CCTransformKeyframe : public CCKeyframe {
public:
    static PassOwnPtr<CCTransformKeyframe> create(double time, const WebKit::WebTransformOperations& value, PassOwnPtr<CCTimingFunction>);
    virtual ~CCTransformKeyframe();

    const WebKit::WebTransformOperations& value() const;

    PassOwnPtr<CCTransformKeyframe> clone() const;

private:
    CCTransformKeyframe(double time, const WebKit::WebTransformOperations& value, PassOwnPtr<CCTimingFunction>);

    WebKit::WebTransformOperations m_value;
};

class CCKeyframedFloatAnimationCurve : public CCFloatAnimationCurve {
public:
    // It is required that the keyframes be sorted by time.
    static PassOwnPtr<CCKeyframedFloatAnimationCurve> create();

    virtual ~CCKeyframedFloatAnimationCurve();

    void addKeyframe(PassOwnPtr<CCFloatKeyframe>);

    // CCAnimationCurve implementation
    virtual double duration() const OVERRIDE;
    virtual PassOwnPtr<CCAnimationCurve> clone() const OVERRIDE;

    // CCFloatAnimationCurve implementation
    virtual float getValue(double t) const OVERRIDE;

private:
    CCKeyframedFloatAnimationCurve();

    // Always sorted in order of increasing time. No two keyframes have the
    // same time.
    Vector<OwnPtr<CCFloatKeyframe> > m_keyframes;
};

class CCKeyframedTransformAnimationCurve : public CCTransformAnimationCurve {
public:
    // It is required that the keyframes be sorted by time.
    static PassOwnPtr<CCKeyframedTransformAnimationCurve> create();

    virtual ~CCKeyframedTransformAnimationCurve();

    void addKeyframe(PassOwnPtr<CCTransformKeyframe>);

    // CCAnimationCurve implementation
    virtual double duration() const OVERRIDE;
    virtual PassOwnPtr<CCAnimationCurve> clone() const OVERRIDE;

    // CCTransformAnimationCurve implementation
    virtual WebKit::WebTransformationMatrix getValue(double t) const OVERRIDE;

private:
    CCKeyframedTransformAnimationCurve();

    // Always sorted in order of increasing time. No two keyframes have the
    // same time.
    Vector<OwnPtr<CCTransformKeyframe> > m_keyframes;
};

} // namespace cc

#endif // CCKeyframedAnimationCurve_h
