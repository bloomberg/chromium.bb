// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCKeyframedAnimationCurve.h"

using WebKit::WebTransformationMatrix;

namespace cc {

namespace {

template <class Keyframe>
void insertKeyframe(scoped_ptr<Keyframe> keyframe, ScopedPtrVector<Keyframe>& keyframes)
{
    // Usually, the keyframes will be added in order, so this loop would be unnecessary and
    // we should skip it if possible.
    if (!keyframes.isEmpty() && keyframe->time() < keyframes.last()->time()) {
        for (size_t i = 0; i < keyframes.size(); ++i) {
            if (keyframe->time() < keyframes[i]->time()) {
                keyframes.insert(i, keyframe.Pass());
                return;
            }
        }
    }

    keyframes.append(keyframe.Pass());
}

scoped_ptr<CCTimingFunction> cloneTimingFunction(const CCTimingFunction* timingFunction)
{
    ASSERT(timingFunction);
    scoped_ptr<CCAnimationCurve> curve(timingFunction->clone());
    return scoped_ptr<CCTimingFunction>(static_cast<CCTimingFunction*>(curve.release()));
}

} // namespace

CCKeyframe::CCKeyframe(double time, scoped_ptr<CCTimingFunction> timingFunction)
    : m_time(time)
    , m_timingFunction(timingFunction.Pass())
{
}

CCKeyframe::~CCKeyframe()
{
}

double CCKeyframe::time() const
{
    return m_time;
}

const CCTimingFunction* CCKeyframe::timingFunction() const
{
    return m_timingFunction.get();
}

scoped_ptr<CCFloatKeyframe> CCFloatKeyframe::create(double time, float value, scoped_ptr<CCTimingFunction> timingFunction)
{
    return make_scoped_ptr(new CCFloatKeyframe(time, value, timingFunction.Pass()));
}

CCFloatKeyframe::CCFloatKeyframe(double time, float value, scoped_ptr<CCTimingFunction> timingFunction)
    : CCKeyframe(time, timingFunction.Pass())
    , m_value(value)
{
}

CCFloatKeyframe::~CCFloatKeyframe()
{
}

float CCFloatKeyframe::value() const
{
    return m_value;
}

scoped_ptr<CCFloatKeyframe> CCFloatKeyframe::clone() const
{
    scoped_ptr<CCTimingFunction> func;
    if (timingFunction())
        func = cloneTimingFunction(timingFunction());
    return CCFloatKeyframe::create(time(), value(), func.Pass());
}

scoped_ptr<CCTransformKeyframe> CCTransformKeyframe::create(double time, const WebKit::WebTransformOperations& value, scoped_ptr<CCTimingFunction> timingFunction)
{
    return make_scoped_ptr(new CCTransformKeyframe(time, value, timingFunction.Pass()));
}

CCTransformKeyframe::CCTransformKeyframe(double time, const WebKit::WebTransformOperations& value, scoped_ptr<CCTimingFunction> timingFunction)
    : CCKeyframe(time, timingFunction.Pass())
    , m_value(value)
{
}

CCTransformKeyframe::~CCTransformKeyframe()
{
}

const WebKit::WebTransformOperations& CCTransformKeyframe::value() const
{
    return m_value;
}

scoped_ptr<CCTransformKeyframe> CCTransformKeyframe::clone() const
{
    scoped_ptr<CCTimingFunction> func;
    if (timingFunction())
        func = cloneTimingFunction(timingFunction());
    return CCTransformKeyframe::create(time(), value(), func.Pass());
}

scoped_ptr<CCKeyframedFloatAnimationCurve> CCKeyframedFloatAnimationCurve::create()
{
    return make_scoped_ptr(new CCKeyframedFloatAnimationCurve);
}

CCKeyframedFloatAnimationCurve::CCKeyframedFloatAnimationCurve()
{
}

CCKeyframedFloatAnimationCurve::~CCKeyframedFloatAnimationCurve()
{
}

void CCKeyframedFloatAnimationCurve::addKeyframe(scoped_ptr<CCFloatKeyframe> keyframe)
{
    insertKeyframe(keyframe.Pass(), m_keyframes);
}

double CCKeyframedFloatAnimationCurve::duration() const
{
    return m_keyframes.last()->time() - m_keyframes.first()->time();
}

scoped_ptr<CCAnimationCurve> CCKeyframedFloatAnimationCurve::clone() const
{
    scoped_ptr<CCKeyframedFloatAnimationCurve> toReturn(CCKeyframedFloatAnimationCurve::create());
    for (size_t i = 0; i < m_keyframes.size(); ++i)
        toReturn->addKeyframe(m_keyframes[i]->clone());
    return toReturn.PassAs<CCAnimationCurve>();
}

float CCKeyframedFloatAnimationCurve::getValue(double t) const
{
    if (t <= m_keyframes.first()->time())
        return m_keyframes.first()->value();

    if (t >= m_keyframes.last()->time())
        return m_keyframes.last()->value();

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

scoped_ptr<CCKeyframedTransformAnimationCurve> CCKeyframedTransformAnimationCurve::create()
{
    return make_scoped_ptr(new CCKeyframedTransformAnimationCurve);
}

CCKeyframedTransformAnimationCurve::CCKeyframedTransformAnimationCurve()
{
}

CCKeyframedTransformAnimationCurve::~CCKeyframedTransformAnimationCurve()
{
}

void CCKeyframedTransformAnimationCurve::addKeyframe(scoped_ptr<CCTransformKeyframe> keyframe)
{
    insertKeyframe(keyframe.Pass(), m_keyframes);
}

double CCKeyframedTransformAnimationCurve::duration() const
{
    return m_keyframes.last()->time() - m_keyframes.first()->time();
}

scoped_ptr<CCAnimationCurve> CCKeyframedTransformAnimationCurve::clone() const
{
    scoped_ptr<CCKeyframedTransformAnimationCurve> toReturn(CCKeyframedTransformAnimationCurve::create());
    for (size_t i = 0; i < m_keyframes.size(); ++i)
        toReturn->addKeyframe(m_keyframes[i]->clone());
    return toReturn.PassAs<CCAnimationCurve>();
}

WebTransformationMatrix CCKeyframedTransformAnimationCurve::getValue(double t) const
{
    if (t <= m_keyframes.first()->time())
        return m_keyframes.first()->value().apply();

    if (t >= m_keyframes.last()->time())
        return m_keyframes.last()->value().apply();

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

} // namespace cc
