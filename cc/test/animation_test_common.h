// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCAnimationTestCommon_h
#define CCAnimationTestCommon_h

#include "CCActiveAnimation.h"
#include "CCAnimationCurve.h"
#include "IntSize.h"
#include "cc/layer_animation_controller.h"

namespace cc {
class LayerImpl;
class Layer;
}

namespace WebKitTests {

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

class FakeLayerAnimationControllerClient : public cc::LayerAnimationControllerClient {
public:
    FakeLayerAnimationControllerClient();
    virtual ~FakeLayerAnimationControllerClient();

    // LayerAnimationControllerClient implementation
    virtual int id() const OVERRIDE;
    virtual void setOpacityFromAnimation(float) OVERRIDE;
    virtual float opacity() const OVERRIDE;
    virtual void setTransformFromAnimation(const WebKit::WebTransformationMatrix&) OVERRIDE;
    virtual const WebKit::WebTransformationMatrix& transform() const OVERRIDE;

private:
    float m_opacity;
    WebKit::WebTransformationMatrix m_transform;
};

void addOpacityTransitionToController(cc::LayerAnimationController&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
void addAnimatedTransformToController(cc::LayerAnimationController&, double duration, int deltaX, int deltaY);

void addOpacityTransitionToLayer(cc::Layer&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
void addOpacityTransitionToLayer(cc::LayerImpl&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);

void addAnimatedTransformToLayer(cc::Layer&, double duration, int deltaX, int deltaY);
void addAnimatedTransformToLayer(cc::LayerImpl&, double duration, int deltaX, int deltaY);

} // namespace WebKitTests

#endif // CCAnimationTesctCommon_h
