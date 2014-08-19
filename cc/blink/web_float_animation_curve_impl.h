// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_FLOAT_ANIMATION_CURVE_IMPL_H_
#define CC_BLINK_WEB_FLOAT_ANIMATION_CURVE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/WebKit/public/platform/WebFloatAnimationCurve.h"

namespace cc {
class AnimationCurve;
class KeyframedFloatAnimationCurve;
}

namespace blink {
struct WebFloatKeyframe;
}

namespace cc_blink {

class WebFloatAnimationCurveImpl : public blink::WebFloatAnimationCurve {
 public:
  CC_BLINK_EXPORT WebFloatAnimationCurveImpl();
  virtual ~WebFloatAnimationCurveImpl();

  // WebCompositorAnimationCurve implementation.
  virtual AnimationCurveType type() const;

  // WebFloatAnimationCurve implementation.
  virtual void add(const blink::WebFloatKeyframe& keyframe);
  virtual void add(const blink::WebFloatKeyframe& keyframe,
                   TimingFunctionType type);
  virtual void add(const blink::WebFloatKeyframe& keyframe,
                   double x1,
                   double y1,
                   double x2,
                   double y2);

  virtual float getValue(double time) const;

  scoped_ptr<cc::AnimationCurve> CloneToAnimationCurve() const;

 private:
  scoped_ptr<cc::KeyframedFloatAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(WebFloatAnimationCurveImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_FLOAT_ANIMATION_CURVE_IMPL_H_
