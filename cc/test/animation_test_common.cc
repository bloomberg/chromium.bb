// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/animation_test_common.h"

#include "cc/keyframed_animation_curve.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_impl.h"
#include "cc/transform_operations.h"

using cc::Animation;
using cc::AnimationCurve;
using cc::EaseTimingFunction;
using cc::FloatKeyframe;
using cc::KeyframedFloatAnimationCurve;
using cc::KeyframedTransformAnimationCurve;
using cc::TimingFunction;
using cc::TransformKeyframe;

namespace cc {

static int nextAnimationId = 0;

template <class Target>
int addOpacityTransition(Target& target, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::Create());

    scoped_ptr<TimingFunction> func;
    if (!useTimingFunction)
        func = EaseTimingFunction::create();
    if (duration > 0)
        curve->AddKeyframe(FloatKeyframe::Create(0, startOpacity, func.Pass()));
    curve->AddKeyframe(FloatKeyframe::Create(duration, endOpacity, scoped_ptr<cc::TimingFunction>()));

    int id = nextAnimationId++;

    scoped_ptr<Animation> animation(Animation::Create(curve.PassAs<AnimationCurve>(), id, 0, Animation::Opacity));
    animation->set_needs_synchronized_start_time(true);

    target.AddAnimation(animation.Pass());
    return id;
}

template <class Target>
int addAnimatedTransform(Target& target, double duration, int deltaX, int deltaY)
{
    scoped_ptr<KeyframedTransformAnimationCurve> curve(KeyframedTransformAnimationCurve::Create());

    if (duration > 0) {
        TransformOperations startOperations;
        startOperations.AppendTranslate(deltaX, deltaY, 0);
        curve->AddKeyframe(TransformKeyframe::Create(0, startOperations, scoped_ptr<cc::TimingFunction>()));
    }

    TransformOperations operations;
    operations.AppendTranslate(deltaX, deltaY, 0);
    curve->AddKeyframe(TransformKeyframe::Create(duration, operations, scoped_ptr<cc::TimingFunction>()));

    int id = nextAnimationId++;

    scoped_ptr<Animation> animation(Animation::Create(curve.PassAs<AnimationCurve>(), id, 0, Animation::Transform));
    animation->set_needs_synchronized_start_time(true);

    target.AddAnimation(animation.Pass());
    return id;
}

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

double FakeFloatAnimationCurve::Duration() const
{
    return m_duration;
}

float FakeFloatAnimationCurve::GetValue(double now) const
{
    return 0;
}

scoped_ptr<cc::AnimationCurve> FakeFloatAnimationCurve::Clone() const
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

double FakeTransformTransition::Duration() const
{
    return m_duration;
}

gfx::Transform FakeTransformTransition::GetValue(double time) const
{
    return gfx::Transform();
}

scoped_ptr<cc::AnimationCurve> FakeTransformTransition::Clone() const
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

double FakeFloatTransition::Duration() const
{
    return m_duration;
}

float FakeFloatTransition::GetValue(double time) const
{
    time /= m_duration;
    if (time >= 1)
        time = 1;
    return (1 - time) * m_from + time * m_to;
}

FakeLayerAnimationValueObserver::FakeLayerAnimationValueObserver()
    : m_opacity(0)
{
}

FakeLayerAnimationValueObserver::~FakeLayerAnimationValueObserver()
{
}

void FakeLayerAnimationValueObserver::OnOpacityAnimated(float opacity)
{
    m_opacity = opacity;
}

void FakeLayerAnimationValueObserver::OnTransformAnimated(const gfx::Transform& transform)
{
    m_transform = transform;
}

bool FakeLayerAnimationValueObserver::IsActive() const
{
    return true;
}

scoped_ptr<cc::AnimationCurve> FakeFloatTransition::Clone() const
{
    return make_scoped_ptr(new FakeFloatTransition(*this)).PassAs<cc::AnimationCurve>();
}

int addOpacityTransitionToController(cc::LayerAnimationController& controller, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    return addOpacityTransition(controller, duration, startOpacity, endOpacity, useTimingFunction);
}

int addAnimatedTransformToController(cc::LayerAnimationController& controller, double duration, int deltaX, int deltaY)
{
    return addAnimatedTransform(controller, duration, deltaX, deltaY);
}

int addOpacityTransitionToLayer(cc::Layer& layer, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    return addOpacityTransition(layer, duration, startOpacity, endOpacity, useTimingFunction);
}

int addOpacityTransitionToLayer(cc::LayerImpl& layer, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    return addOpacityTransition(*layer.layer_animation_controller(), duration, startOpacity, endOpacity, useTimingFunction);
}

int addAnimatedTransformToLayer(cc::Layer& layer, double duration, int deltaX, int deltaY)
{
    return addAnimatedTransform(layer, duration, deltaX, deltaY);
}

int addAnimatedTransformToLayer(cc::LayerImpl& layer, double duration, int deltaX, int deltaY)
{
    return addAnimatedTransform(*layer.layer_animation_controller(), duration, deltaX, deltaY);
}

}  // namespace cc
