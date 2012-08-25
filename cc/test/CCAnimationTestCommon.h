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

namespace WebCore {
class CCLayerImpl;
class LayerChromium;
}

namespace WebKitTests {

class FakeFloatAnimationCurve : public WebCore::CCFloatAnimationCurve {
public:
    FakeFloatAnimationCurve();
    virtual ~FakeFloatAnimationCurve();

    virtual double duration() const OVERRIDE { return 1; }
    virtual float getValue(double now) const OVERRIDE { return 0; }
    virtual PassOwnPtr<WebCore::CCAnimationCurve> clone() const OVERRIDE;
};

class FakeTransformTransition : public WebCore::CCTransformAnimationCurve {
public:
    FakeTransformTransition(double duration);
    virtual ~FakeTransformTransition();

    virtual double duration() const OVERRIDE { return m_duration; }
    virtual WebKit::WebTransformationMatrix getValue(double time) const OVERRIDE;

    virtual PassOwnPtr<WebCore::CCAnimationCurve> clone() const OVERRIDE;

private:
    double m_duration;
};

class FakeFloatTransition : public WebCore::CCFloatAnimationCurve {
public:
    FakeFloatTransition(double duration, float from, float to);
    virtual ~FakeFloatTransition();

    virtual double duration() const OVERRIDE { return m_duration; }
    virtual float getValue(double time) const OVERRIDE;

    virtual PassOwnPtr<WebCore::CCAnimationCurve> clone() const OVERRIDE;

private:
    double m_duration;
    float m_from;
    float m_to;
};

class FakeLayerAnimationControllerClient : public WebCore::CCLayerAnimationControllerClient {
public:
    FakeLayerAnimationControllerClient();
    virtual ~FakeLayerAnimationControllerClient();

    // CCLayerAnimationControllerClient implementation
    virtual int id() const OVERRIDE { return 0; }
    virtual void setOpacityFromAnimation(float opacity) OVERRIDE { m_opacity = opacity; }
    virtual float opacity() const OVERRIDE { return m_opacity; }
    virtual void setTransformFromAnimation(const WebKit::WebTransformationMatrix& transform) OVERRIDE { m_transform = transform; }
    virtual const WebKit::WebTransformationMatrix& transform() const OVERRIDE { return m_transform; }

private:
    float m_opacity;
    WebKit::WebTransformationMatrix m_transform;
};

void addOpacityTransitionToController(WebCore::CCLayerAnimationController&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
void addAnimatedTransformToController(WebCore::CCLayerAnimationController&, double duration, int deltaX, int deltaY);

void addOpacityTransitionToLayer(WebCore::LayerChromium&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
void addOpacityTransitionToLayer(WebCore::CCLayerImpl&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);

void addAnimatedTransformToLayer(WebCore::LayerChromium&, double duration, int deltaX, int deltaY);
void addAnimatedTransformToLayer(WebCore::CCLayerImpl&, double duration, int deltaX, int deltaY);

} // namespace WebKitTests

#endif // CCAnimationTesctCommon_h
