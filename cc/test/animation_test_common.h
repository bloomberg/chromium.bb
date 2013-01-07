// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ANIMATION_TEST_COMMON_H_
#define CC_TEST_ANIMATION_TEST_COMMON_H_

#include "cc/active_animation.h"
#include "cc/animation_curve.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_animation_value_observer.h"

namespace cc {
class LayerImpl;
class Layer;
}

namespace cc {

class FakeFloatAnimationCurve : public cc::FloatAnimationCurve {
public:
    FakeFloatAnimationCurve();
    explicit FakeFloatAnimationCurve(double duration);
    virtual ~FakeFloatAnimationCurve();

    virtual double duration() const OVERRIDE;
    virtual float getValue(double now) const OVERRIDE;
    virtual scoped_ptr<cc::AnimationCurve> clone() const OVERRIDE;

private:
    double m_duration;
};

class FakeTransformTransition : public cc::TransformAnimationCurve {
public:
    FakeTransformTransition(double duration);
    virtual ~FakeTransformTransition();

    virtual double duration() const OVERRIDE;
    virtual WebKit::WebTransformationMatrix getValue(double time) const OVERRIDE;

    virtual scoped_ptr<cc::AnimationCurve> clone() const OVERRIDE;

private:
    double m_duration;
};

class FakeFloatTransition : public cc::FloatAnimationCurve {
public:
    FakeFloatTransition(double duration, float from, float to);
    virtual ~FakeFloatTransition();

    virtual double duration() const OVERRIDE;
    virtual float getValue(double time) const OVERRIDE;

    virtual scoped_ptr<cc::AnimationCurve> clone() const OVERRIDE;

private:
    double m_duration;
    float m_from;
    float m_to;
};

class FakeLayerAnimationValueObserver : public cc::LayerAnimationValueObserver {
public:
    FakeLayerAnimationValueObserver();
    virtual ~FakeLayerAnimationValueObserver();

    // LayerAnimationValueObserver implementation
    virtual void OnOpacityAnimated(float) OVERRIDE;
    virtual void OnTransformAnimated(const gfx::Transform&) OVERRIDE;
    virtual bool IsActive() const OVERRIDE;

    float opacity() const  { return m_opacity; }
    const gfx::Transform& transform() const { return m_transform; }

private:
    float m_opacity;
    gfx::Transform m_transform;
};

int addOpacityTransitionToController(cc::LayerAnimationController&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
int addAnimatedTransformToController(cc::LayerAnimationController&, double duration, int deltaX, int deltaY);

int addOpacityTransitionToLayer(cc::Layer&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
int addOpacityTransitionToLayer(cc::LayerImpl&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);

int addAnimatedTransformToLayer(cc::Layer&, double duration, int deltaX, int deltaY);
int addAnimatedTransformToLayer(cc::LayerImpl&, double duration, int deltaX, int deltaY);

}  // namespace cc

#endif  // CC_TEST_ANIMATION_TEST_COMMON_H_
