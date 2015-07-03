// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_COMPOSITOR_ANIMATION_TIMELINE_IMPL_H_
#define CC_BLINK_WEB_COMPOSITOR_ANIMATION_TIMELINE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/WebKit/public/platform/WebCompositorAnimationTimeline.h"

namespace blink {
class WebCompositorAnimationPlayerClient;
}

namespace cc {
class AnimationTimeline;
}

namespace cc_blink {

class WebCompositorAnimationTimelineImpl
    : public blink::WebCompositorAnimationTimeline {
 public:
  CC_BLINK_EXPORT explicit WebCompositorAnimationTimelineImpl();
  virtual ~WebCompositorAnimationTimelineImpl();

  CC_BLINK_EXPORT cc::AnimationTimeline* animation_timeline() const;

  // blink::WebCompositorAnimationTimeline implementation
  virtual void playerAttached(
      const blink::WebCompositorAnimationPlayerClient& client);
  virtual void playerDestroyed(
      const blink::WebCompositorAnimationPlayerClient& client);

 private:
  scoped_refptr<cc::AnimationTimeline> animation_timeline_;

  DISALLOW_COPY_AND_ASSIGN(WebCompositorAnimationTimelineImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_COMPOSITOR_ANIMATION_TIMELINE_IMPL_H_
