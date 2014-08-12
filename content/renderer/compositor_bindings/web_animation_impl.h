// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebCompositorAnimation.h"
#include "third_party/WebKit/public/platform/WebCompositorAnimationCurve.h"

namespace cc {
class Animation;
}

namespace content {

class WebCompositorAnimationImpl : public blink::WebCompositorAnimation {
 public:
  CONTENT_EXPORT WebCompositorAnimationImpl(
      const blink::WebCompositorAnimationCurve& curve,
      TargetProperty target,
      int animation_id,
      int group_id);
  virtual ~WebCompositorAnimationImpl();

  // blink::WebCompositorAnimation implementation
  virtual int id();
  virtual TargetProperty targetProperty() const;
#if WEB_ANIMATION_SUPPORTS_FRACTIONAL_ITERATIONS
  virtual double iterations() const;
  virtual void setIterations(double iterations);
#else
  virtual int iterations() const;
  virtual void setIterations(int iterations);
#endif
  virtual double startTime() const;
  virtual void setStartTime(double monotonic_time);
  virtual double timeOffset() const;
  virtual void setTimeOffset(double monotonic_time);
#if WEB_ANIMATION_SUPPORTS_FULL_DIRECTION
  virtual Direction direction() const;
  virtual void setDirection(Direction);
#else
  virtual bool alternatesDirection() const;
  virtual void setAlternatesDirection(bool alternates);
#endif

  scoped_ptr<cc::Animation> PassAnimation();

 private:
  scoped_ptr<cc::Animation> animation_;

  DISALLOW_COPY_AND_ASSIGN(WebCompositorAnimationImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_IMPL_H_

