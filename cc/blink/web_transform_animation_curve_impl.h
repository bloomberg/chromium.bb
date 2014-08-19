// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_TRANSFORM_ANIMATION_CURVE_IMPL_H_
#define CC_BLINK_WEB_TRANSFORM_ANIMATION_CURVE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/WebKit/public/platform/WebTransformAnimationCurve.h"

namespace cc {
class AnimationCurve;
class KeyframedTransformAnimationCurve;
}

namespace blink {
class WebTransformKeyframe;
}

namespace cc_blink {

class WebTransformAnimationCurveImpl
    : public blink::WebTransformAnimationCurve {
 public:
  CC_BLINK_EXPORT WebTransformAnimationCurveImpl();
  virtual ~WebTransformAnimationCurveImpl();

  // blink::WebCompositorAnimationCurve implementation.
  virtual AnimationCurveType type() const;

  // blink::WebTransformAnimationCurve implementation.
  virtual void add(const blink::WebTransformKeyframe& keyframe);
  virtual void add(const blink::WebTransformKeyframe& keyframe,
                   TimingFunctionType type);
  virtual void add(const blink::WebTransformKeyframe& keyframe,
                   double x1,
                   double y1,
                   double x2,
                   double y2);

  scoped_ptr<cc::AnimationCurve> CloneToAnimationCurve() const;

 private:
  scoped_ptr<cc::KeyframedTransformAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(WebTransformAnimationCurveImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_TRANSFORM_ANIMATION_CURVE_IMPL_H_
