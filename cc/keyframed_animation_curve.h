// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCKeyframedAnimationCurve_h
#define CCKeyframedAnimationCurve_h

#include "CCAnimationCurve.h"
#include "cc/timing_function.h"
#include "cc/scoped_ptr_vector.h"
#include <public/WebTransformOperations.h>

namespace cc {

class Keyframe {
public:
    double time() const;
    const TimingFunction* timingFunction() const;

protected:
    Keyframe(double time, scoped_ptr<TimingFunction>);
    virtual ~Keyframe();

private:
    double m_time;
    scoped_ptr<TimingFunction> m_timingFunction;
};

class FloatKeyframe : public Keyframe {
public:
    static scoped_ptr<FloatKeyframe> create(double time, float value, scoped_ptr<TimingFunction>);
    virtual ~FloatKeyframe();

    float value() const;

    scoped_ptr<FloatKeyframe> clone() const;

private:
    FloatKeyframe(double time, float value, scoped_ptr<TimingFunction>);

    float m_value;
};

class TransformKeyframe : public Keyframe {
public:
    static scoped_ptr<TransformKeyframe> create(double time, const WebKit::WebTransformOperations& value, scoped_ptr<TimingFunction>);
    virtual ~TransformKeyframe();

    const WebKit::WebTransformOperations& value() const;

    scoped_ptr<TransformKeyframe> clone() const;

private:
    TransformKeyframe(double time, const WebKit::WebTransformOperations& value, scoped_ptr<TimingFunction>);

    WebKit::WebTransformOperations m_value;
};

class KeyframedFloatAnimationCurve : public FloatAnimationCurve {
public:
    // It is required that the keyframes be sorted by time.
    static scoped_ptr<KeyframedFloatAnimationCurve> create();

    virtual ~KeyframedFloatAnimationCurve();

    void addKeyframe(scoped_ptr<FloatKeyframe>);

    // AnimationCurve implementation
    virtual double duration() const OVERRIDE;
    virtual scoped_ptr<AnimationCurve> clone() const OVERRIDE;

    // FloatAnimationCurve implementation
    virtual float getValue(double t) const OVERRIDE;

private:
    KeyframedFloatAnimationCurve();

    // Always sorted in order of increasing time. No two keyframes have the
    // same time.
    ScopedPtrVector<FloatKeyframe> m_keyframes;
};

class KeyframedTransformAnimationCurve : public TransformAnimationCurve {
public:
    // It is required that the keyframes be sorted by time.
    static scoped_ptr<KeyframedTransformAnimationCurve> create();

    virtual ~KeyframedTransformAnimationCurve();

    void addKeyframe(scoped_ptr<TransformKeyframe>);

    // AnimationCurve implementation
    virtual double duration() const OVERRIDE;
    virtual scoped_ptr<AnimationCurve> clone() const OVERRIDE;

    // TransformAnimationCurve implementation
    virtual WebKit::WebTransformationMatrix getValue(double t) const OVERRIDE;

private:
    KeyframedTransformAnimationCurve();

    // Always sorted in order of increasing time. No two keyframes have the
    // same time.
    ScopedPtrVector<TransformKeyframe> m_keyframes;
};

} // namespace cc

#endif // CCKeyframedAnimationCurve_h
