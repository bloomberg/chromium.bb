// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCAnimationTestCommon.h"

#include "CCKeyframedAnimationCurve.h"
#include "CCLayerAnimationController.h"
#include "CCLayerImpl.h"
#include "LayerChromium.h"
#include <public/WebTransformOperations.h>

using namespace WebCore;

namespace {

template <class Target>
void addOpacityTransition(Target& target, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    OwnPtr<CCKeyframedFloatAnimationCurve> curve(CCKeyframedFloatAnimationCurve::create());

    if (duration > 0)
        curve->addKeyframe(CCFloatKeyframe::create(0, startOpacity, useTimingFunction ? nullptr : CCEaseTimingFunction::create()));
    curve->addKeyframe(CCFloatKeyframe::create(duration, endOpacity, nullptr));

    OwnPtr<CCActiveAnimation> animation(CCActiveAnimation::create(curve.release(), 0, 0, CCActiveAnimation::Opacity));
    animation->setNeedsSynchronizedStartTime(true);

    target.addAnimation(animation.release());
}

template <class Target>
void addAnimatedTransform(Target& target, double duration, int deltaX, int deltaY)
{
    static int id = 0;
    OwnPtr<CCKeyframedTransformAnimationCurve> curve(CCKeyframedTransformAnimationCurve::create());

    if (duration > 0) {
        WebKit::WebTransformOperations startOperations;
        startOperations.appendTranslate(deltaX, deltaY, 0);
        curve->addKeyframe(CCTransformKeyframe::create(0, startOperations, nullptr));
    }

    WebKit::WebTransformOperations operations;
    operations.appendTranslate(deltaX, deltaY, 0);
    curve->addKeyframe(CCTransformKeyframe::create(duration, operations, nullptr));

    OwnPtr<CCActiveAnimation> animation(CCActiveAnimation::create(curve.release(), id++, 0, CCActiveAnimation::Transform));
    animation->setNeedsSynchronizedStartTime(true);

    target.addAnimation(animation.release());
}

} // namespace

namespace WebKitTests {

FakeFloatAnimationCurve::FakeFloatAnimationCurve()
{
}

FakeFloatAnimationCurve::~FakeFloatAnimationCurve()
{
}

PassOwnPtr<WebCore::CCAnimationCurve> FakeFloatAnimationCurve::clone() const
{
    return adoptPtr(new FakeFloatAnimationCurve);
}

FakeTransformTransition::FakeTransformTransition(double duration)
    : m_duration(duration)
{
}

FakeTransformTransition::~FakeTransformTransition()
{
}

WebKit::WebTransformationMatrix FakeTransformTransition::getValue(double time) const
{
    return WebKit::WebTransformationMatrix();
}

PassOwnPtr<WebCore::CCAnimationCurve> FakeTransformTransition::clone() const
{
    return adoptPtr(new FakeTransformTransition(*this));
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

PassOwnPtr<WebCore::CCAnimationCurve> FakeFloatTransition::clone() const
{
    return adoptPtr(new FakeFloatTransition(*this));
}

void addOpacityTransitionToController(WebCore::CCLayerAnimationController& controller, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    addOpacityTransition(controller, duration, startOpacity, endOpacity, useTimingFunction);
}

void addAnimatedTransformToController(WebCore::CCLayerAnimationController& controller, double duration, int deltaX, int deltaY)
{
    addAnimatedTransform(controller, duration, deltaX, deltaY);
}

void addOpacityTransitionToLayer(WebCore::LayerChromium& layer, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    addOpacityTransition(layer, duration, startOpacity, endOpacity, useTimingFunction);
}

void addOpacityTransitionToLayer(WebCore::CCLayerImpl& layer, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    addOpacityTransition(*layer.layerAnimationController(), duration, startOpacity, endOpacity, useTimingFunction);
}

void addAnimatedTransformToLayer(WebCore::LayerChromium& layer, double duration, int deltaX, int deltaY)
{
    addAnimatedTransform(layer, duration, deltaX, deltaY);
}

void addAnimatedTransformToLayer(WebCore::CCLayerImpl& layer, double duration, int deltaX, int deltaY)
{
    addAnimatedTransform(*layer.layerAnimationController(), duration, deltaX, deltaY);
}

} // namespace WebKitTests
