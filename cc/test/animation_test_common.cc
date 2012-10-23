// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/test/animation_test_common.h"

#include "cc/keyframed_animation_curve.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_impl.h"
#include <public/WebTransformOperations.h>

using namespace cc;

namespace {

template <class Target>
void addOpacityTransition(Target& target, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::create());

    scoped_ptr<TimingFunction> func;
    if (!useTimingFunction)
        func = EaseTimingFunction::create();
    if (duration > 0)
        curve->addKeyframe(FloatKeyframe::create(0, startOpacity, func.Pass()));
    curve->addKeyframe(FloatKeyframe::create(duration, endOpacity, scoped_ptr<cc::TimingFunction>()));

    scoped_ptr<ActiveAnimation> animation(ActiveAnimation::create(curve.PassAs<AnimationCurve>(), 0, 0, ActiveAnimation::Opacity));
    animation->setNeedsSynchronizedStartTime(true);

    target.addAnimation(animation.Pass());
}

template <class Target>
void addAnimatedTransform(Target& target, double duration, int deltaX, int deltaY)
{
    static int id = 0;
    scoped_ptr<KeyframedTransformAnimationCurve> curve(KeyframedTransformAnimationCurve::create());

    if (duration > 0) {
        WebKit::WebTransformOperations startOperations;
        startOperations.appendTranslate(deltaX, deltaY, 0);
        curve->addKeyframe(TransformKeyframe::create(0, startOperations, scoped_ptr<cc::TimingFunction>()));
    }

    WebKit::WebTransformOperations operations;
    operations.appendTranslate(deltaX, deltaY, 0);
    curve->addKeyframe(TransformKeyframe::create(duration, operations, scoped_ptr<cc::TimingFunction>()));

    scoped_ptr<ActiveAnimation> animation(ActiveAnimation::create(curve.PassAs<AnimationCurve>(), id++, 0, ActiveAnimation::Transform));
    animation->setNeedsSynchronizedStartTime(true);

    target.addAnimation(animation.Pass());
}

} // namespace

namespace WebKitTests {

FakeFloatAnimationCurve::FakeFloatAnimationCurve()
    : m_duration(1)
{
}

FakeFloatAnimationCurve::FakeFloatAnimationCurve(double duration)
    : m_duration(duration)
{
}

FakeFloatAnimationCurve::~FakeFloatAnimationCurve()
{
}

double FakeFloatAnimationCurve::duration() const
{
    return m_duration;
}

float FakeFloatAnimationCurve::getValue(double now) const
{
    return 0;
}

scoped_ptr<cc::AnimationCurve> FakeFloatAnimationCurve::clone() const
{
    return make_scoped_ptr(new FakeFloatAnimationCurve).PassAs<cc::AnimationCurve>();
}

FakeTransformTransition::FakeTransformTransition(double duration)
    : m_duration(duration)
{
}

FakeTransformTransition::~FakeTransformTransition()
{
}

double FakeTransformTransition::duration() const
{
    return m_duration;
}

WebKit::WebTransformationMatrix FakeTransformTransition::getValue(double time) const
{
    return WebKit::WebTransformationMatrix();
}

scoped_ptr<cc::AnimationCurve> FakeTransformTransition::clone() const
{
    return make_scoped_ptr(new FakeTransformTransition(*this)).PassAs<cc::AnimationCurve>();
}


FakeFloatTransition::FakeFloatTransition(double duration, float from, float to)
    : m_duration(duration)
    , m_from(from)
    , m_to(to)
{
}

FakeFloatTransition::~FakeFloatTransition()
{
}

double FakeFloatTransition::duration() const
{
    return m_duration;
}

float FakeFloatTransition::getValue(double time) const
{
    time /= m_duration;
    if (time >= 1)
        time = 1;
    return (1 - time) * m_from + time * m_to;
}

FakeLayerAnimationControllerClient::FakeLayerAnimationControllerClient()
    : m_opacity(0)
{
}

FakeLayerAnimationControllerClient::~FakeLayerAnimationControllerClient()
{
}

int FakeLayerAnimationControllerClient::id() const
{
    return 0;
}

void FakeLayerAnimationControllerClient::setOpacityFromAnimation(float opacity)
{
    m_opacity = opacity;
}

float FakeLayerAnimationControllerClient::opacity() const
{
    return m_opacity;
}

void FakeLayerAnimationControllerClient::setTransformFromAnimation(const WebKit::WebTransformationMatrix& transform)
{
    m_transform = transform;
}

const WebKit::WebTransformationMatrix& FakeLayerAnimationControllerClient::transform() const
{
    return m_transform;
}

scoped_ptr<cc::AnimationCurve> FakeFloatTransition::clone() const
{
    return make_scoped_ptr(new FakeFloatTransition(*this)).PassAs<cc::AnimationCurve>();
}

void addOpacityTransitionToController(cc::LayerAnimationController& controller, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    addOpacityTransition(controller, duration, startOpacity, endOpacity, useTimingFunction);
}

void addAnimatedTransformToController(cc::LayerAnimationController& controller, double duration, int deltaX, int deltaY)
{
    addAnimatedTransform(controller, duration, deltaX, deltaY);
}

void addOpacityTransitionToLayer(cc::Layer& layer, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    addOpacityTransition(layer, duration, startOpacity, endOpacity, useTimingFunction);
}

void addOpacityTransitionToLayer(cc::LayerImpl& layer, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    addOpacityTransition(*layer.layerAnimationController(), duration, startOpacity, endOpacity, useTimingFunction);
}

void addAnimatedTransformToLayer(cc::Layer& layer, double duration, int deltaX, int deltaY)
{
    addAnimatedTransform(layer, duration, deltaX, deltaY);
}

void addAnimatedTransformToLayer(cc::LayerImpl& layer, double duration, int deltaX, int deltaY)
{
    addAnimatedTransform(*layer.layerAnimationController(), duration, deltaX, deltaY);
}

} // namespace WebKitTests
