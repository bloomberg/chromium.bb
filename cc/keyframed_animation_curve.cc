// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/keyframed_animation_curve.h"

using WebKit::WebTransformationMatrix;

namespace cc {

namespace {

template <class Keyframe>
void insertKeyframe(scoped_ptr<Keyframe> keyframe, ScopedPtrVector<Keyframe>& keyframes)
{
    // Usually, the keyframes will be added in order, so this loop would be unnecessary and
    // we should skip it if possible.
    if (!keyframes.empty() && keyframe->time() < keyframes.back()->time()) {
        for (size_t i = 0; i < keyframes.size(); ++i) {
            if (keyframe->time() < keyframes[i]->time()) {
                keyframes.insert(keyframes.begin() + i, keyframe.Pass());
                return;
            }
        }
    }

    keyframes.push_back(keyframe.Pass());
}

scoped_ptr<TimingFunction> cloneTimingFunction(const TimingFunction* timingFunction)
{
    DCHECK(timingFunction);
    scoped_ptr<AnimationCurve> curve(timingFunction->clone());
    return scoped_ptr<TimingFunction>(static_cast<TimingFunction*>(curve.release()));
}

} // namespace

Keyframe::Keyframe(double time, scoped_ptr<TimingFunction> timingFunction)
    : m_time(time)
    , m_timingFunction(timingFunction.Pass())
{
}

Keyframe::~Keyframe()
{
}

double Keyframe::time() const
{
    return m_time;
}

const TimingFunction* Keyframe::timingFunction() const
{
    return m_timingFunction.get();
}

scoped_ptr<FloatKeyframe> FloatKeyframe::create(double time, float value, scoped_ptr<TimingFunction> timingFunction)
{
    return make_scoped_ptr(new FloatKeyframe(time, value, timingFunction.Pass()));
}

FloatKeyframe::FloatKeyframe(double time, float value, scoped_ptr<TimingFunction> timingFunction)
    : Keyframe(time, timingFunction.Pass())
    , m_value(value)
{
}

FloatKeyframe::~FloatKeyframe()
{
}

float FloatKeyframe::value() const
{
    return m_value;
}

scoped_ptr<FloatKeyframe> FloatKeyframe::clone() const
{
    scoped_ptr<TimingFunction> func;
    if (timingFunction())
        func = cloneTimingFunction(timingFunction());
    return FloatKeyframe::create(time(), value(), func.Pass());
}

scoped_ptr<TransformKeyframe> TransformKeyframe::create(double time, const WebKit::WebTransformOperations& value, scoped_ptr<TimingFunction> timingFunction)
{
    return make_scoped_ptr(new TransformKeyframe(time, value, timingFunction.Pass()));
}

TransformKeyframe::TransformKeyframe(double time, const WebKit::WebTransformOperations& value, scoped_ptr<TimingFunction> timingFunction)
    : Keyframe(time, timingFunction.Pass())
    , m_value(value)
{
}

TransformKeyframe::~TransformKeyframe()
{
}

const WebKit::WebTransformOperations& TransformKeyframe::value() const
{
    return m_value;
}

scoped_ptr<TransformKeyframe> TransformKeyframe::clone() const
{
    scoped_ptr<TimingFunction> func;
    if (timingFunction())
        func = cloneTimingFunction(timingFunction());
    return TransformKeyframe::create(time(), value(), func.Pass());
}

scoped_ptr<KeyframedFloatAnimationCurve> KeyframedFloatAnimationCurve::create()
{
    return make_scoped_ptr(new KeyframedFloatAnimationCurve);
}

KeyframedFloatAnimationCurve::KeyframedFloatAnimationCurve()
{
}

KeyframedFloatAnimationCurve::~KeyframedFloatAnimationCurve()
{
}

void KeyframedFloatAnimationCurve::addKeyframe(scoped_ptr<FloatKeyframe> keyframe)
{
    insertKeyframe(keyframe.Pass(), m_keyframes);
}

double KeyframedFloatAnimationCurve::duration() const
{
    return m_keyframes.back()->time() - m_keyframes.front()->time();
}

scoped_ptr<AnimationCurve> KeyframedFloatAnimationCurve::clone() const
{
    scoped_ptr<KeyframedFloatAnimationCurve> toReturn(KeyframedFloatAnimationCurve::create());
    for (size_t i = 0; i < m_keyframes.size(); ++i)
        toReturn->addKeyframe(m_keyframes[i]->clone());
    return toReturn.PassAs<AnimationCurve>();
}

float KeyframedFloatAnimationCurve::getValue(double t) const
{
    if (t <= m_keyframes.front()->time())
        return m_keyframes.front()->value();

    if (t >= m_keyframes.back()->time())
        return m_keyframes.back()->value();

    size_t i = 0;
    for (; i < m_keyframes.size() - 1; ++i) {
        if (t < m_keyframes[i+1]->time())
            break;
    }

    float progress = static_cast<float>((t - m_keyframes[i]->time()) / (m_keyframes[i+1]->time() - m_keyframes[i]->time()));

    if (m_keyframes[i]->timingFunction())
        progress = m_keyframes[i]->timingFunction()->getValue(progress);

    return m_keyframes[i]->value() + (m_keyframes[i+1]->value() - m_keyframes[i]->value()) * progress;
}

scoped_ptr<KeyframedTransformAnimationCurve> KeyframedTransformAnimationCurve::create()
{
    return make_scoped_ptr(new KeyframedTransformAnimationCurve);
}

KeyframedTransformAnimationCurve::KeyframedTransformAnimationCurve()
{
}

KeyframedTransformAnimationCurve::~KeyframedTransformAnimationCurve()
{
}

void KeyframedTransformAnimationCurve::addKeyframe(scoped_ptr<TransformKeyframe> keyframe)
{
    insertKeyframe(keyframe.Pass(), m_keyframes);
}

double KeyframedTransformAnimationCurve::duration() const
{
    return m_keyframes.back()->time() - m_keyframes.front()->time();
}

scoped_ptr<AnimationCurve> KeyframedTransformAnimationCurve::clone() const
{
    scoped_ptr<KeyframedTransformAnimationCurve> toReturn(KeyframedTransformAnimationCurve::create());
    for (size_t i = 0; i < m_keyframes.size(); ++i)
        toReturn->addKeyframe(m_keyframes[i]->clone());
    return toReturn.PassAs<AnimationCurve>();
}

WebTransformationMatrix KeyframedTransformAnimationCurve::getValue(double t) const
{
    if (t <= m_keyframes.front()->time())
        return m_keyframes.front()->value().apply();

    if (t >= m_keyframes.back()->time())
        return m_keyframes.back()->value().apply();

    size_t i = 0;
    for (; i < m_keyframes.size() - 1; ++i) {
        if (t < m_keyframes[i+1]->time())
            break;
    }

    double progress = (t - m_keyframes[i]->time()) / (m_keyframes[i+1]->time() - m_keyframes[i]->time());

    if (m_keyframes[i]->timingFunction())
        progress = m_keyframes[i]->timingFunction()->getValue(progress);

    return m_keyframes[i+1]->value().blend(m_keyframes[i]->value(), progress);
}

}  // namespace cc
