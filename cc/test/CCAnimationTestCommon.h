// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCAnimationTestCommon_h
#define CCAnimationTestCommon_h

#include "CCActiveAnimation.h"
#include "CCAnimationCurve.h"
#include "CCLayerAnimationController.h"
#include "IntSize.h"

#include <wtf/OwnPtr.h>

namespace cc {
class CCLayerImpl;
class LayerChromium;
}

namespace WebKitTests {

class FakeFloatAnimationCurve : public cc::CCFloatAnimationCurve {
public:
    FakeFloatAnimationCurve();
    virtual ~FakeFloatAnimationCurve();

    virtual double duration() const OVERRIDE;
    virtual float getValue(double now) const OVERRIDE;
    virtual PassOwnPtr<cc::CCAnimationCurve> clone() const OVERRIDE;
};

class FakeTransformTransition : public cc::CCTransformAnimationCurve {
public:
    FakeTransformTransition(double duration);
    virtual ~FakeTransformTransition();

    virtual double duration() const OVERRIDE;
    virtual WebKit::WebTransformationMatrix getValue(double time) const OVERRIDE;

    virtual PassOwnPtr<cc::CCAnimationCurve> clone() const OVERRIDE;

private:
    double m_duration;
};

class FakeFloatTransition : public cc::CCFloatAnimationCurve {
public:
    FakeFloatTransition(double duration, float from, float to);
    virtual ~FakeFloatTransition();

    virtual double duration() const OVERRIDE;
    virtual float getValue(double time) const OVERRIDE;

    virtual PassOwnPtr<cc::CCAnimationCurve> clone() const OVERRIDE;

private:
    double m_duration;
    float m_from;
    float m_to;
};

class FakeLayerAnimationControllerClient : public cc::CCLayerAnimationControllerClient {
public:
    FakeLayerAnimationControllerClient();
    virtual ~FakeLayerAnimationControllerClient();

    // CCLayerAnimationControllerClient implementation
    virtual int id() const OVERRIDE;
    virtual void setOpacityFromAnimation(float) OVERRIDE;
    virtual float opacity() const OVERRIDE;
    virtual void setTransformFromAnimation(const WebKit::WebTransformationMatrix&) OVERRIDE;
    virtual const WebKit::WebTransformationMatrix& transform() const OVERRIDE;

private:
    float m_opacity;
    WebKit::WebTransformationMatrix m_transform;
};

void addOpacityTransitionToController(cc::CCLayerAnimationController&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
void addAnimatedTransformToController(cc::CCLayerAnimationController&, double duration, int deltaX, int deltaY);

void addOpacityTransitionToLayer(cc::LayerChromium&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
void addOpacityTransitionToLayer(cc::CCLayerImpl&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);

void addAnimatedTransformToLayer(cc::LayerChromium&, double duration, int deltaX, int deltaY);
void addAnimatedTransformToLayer(cc::CCLayerImpl&, double duration, int deltaX, int deltaY);

} // namespace WebKitTests

#endif // CCAnimationTesctCommon_h
