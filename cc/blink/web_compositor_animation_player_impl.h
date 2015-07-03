// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_COMPOSITOR_ANIMATION_PLAYER_IMPL_H_
#define CC_BLINK_WEB_COMPOSITOR_ANIMATION_PLAYER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/WebKit/public/platform/WebCompositorAnimationPlayer.h"

namespace cc {
class AnimationPlayer;
}

namespace cc_blink {

class WebToCCAnimationDelegateAdapter;

class WebCompositorAnimationPlayerImpl
    : public blink::WebCompositorAnimationPlayer {
 public:
  CC_BLINK_EXPORT WebCompositorAnimationPlayerImpl();
  virtual ~WebCompositorAnimationPlayerImpl();

  CC_BLINK_EXPORT cc::AnimationPlayer* animation_player() const;

  // blink::WebCompositorAnimationPlayer implementation
  virtual void setAnimationDelegate(
      blink::WebCompositorAnimationDelegate* delegate);
  virtual void attachLayer(blink::WebLayer* web_layer);
  virtual void detachLayer();
  virtual bool isLayerAttached() const;
  virtual void addAnimation(blink::WebCompositorAnimation* animation);
  virtual void removeAnimation(int animation_id);
  virtual void pauseAnimation(int animation_id, double time_offset);

 private:
  scoped_refptr<cc::AnimationPlayer> animation_player_;
  scoped_ptr<WebToCCAnimationDelegateAdapter> animation_delegate_adapter_;

  DISALLOW_COPY_AND_ASSIGN(WebCompositorAnimationPlayerImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_COMPOSITOR_ANIMATION_PLAYER_IMPL_H_
