// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TOP_CONTROLS_MANAGER_H_
#define CC_TOP_CONTROLS_MANAGER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/layer_impl.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d_f.h"

namespace base {
class TimeTicks;
}

namespace cc {

class KeyframedFloatAnimationCurve;
class LayerTreeImpl;
class TopControlsManagerClient;

// Manages the position of the top controls.
class CC_EXPORT TopControlsManager {
 public:
  enum AnimationDirection {
    NO_ANIMATION,
    SHOWING_CONTROLS,
    HIDING_CONTROLS
  };

  static scoped_ptr<TopControlsManager> Create(
      TopControlsManagerClient* client,
      float top_controls_height,
      float top_controls_show_threshold,
      float top_controls_hide_threshold);
  virtual ~TopControlsManager();

  float controls_top_offset() { return controls_top_offset_; }
  float content_top_offset() { return content_top_offset_; }
  KeyframedFloatAnimationCurve* animation() {
    return top_controls_animation_.get();
  }
  AnimationDirection animation_direction() { return animation_direction_; }

  void ScrollBegin();
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF pending_delta);
  void ScrollEnd();

  void Animate(base::TimeTicks monotonic_time);

 protected:
  TopControlsManager(TopControlsManagerClient* client,
                     float top_controls_height,
                     float top_controls_show_threshold,
                     float top_controls_hide_threshold);

 private:
  gfx::Vector2dF ScrollInternal(const gfx::Vector2dF pending_delta);
  void ResetAnimations();
  float RootScrollLayerTotalScrollY();
  void SetupAnimation(AnimationDirection direction);
  void StartAnimationIfNecessary();
  bool IsAnimationCompleteAtTime(base::TimeTicks time);

  TopControlsManagerClient* client_;  // The client manages the lifecycle of
                                      // this.

  scoped_ptr<KeyframedFloatAnimationCurve> top_controls_animation_;
  AnimationDirection animation_direction_;
  bool in_scroll_gesture_;
  float controls_top_offset_;
  float content_top_offset_;
  float top_controls_height_;
  float previous_root_scroll_offset_;
  float scroll_start_offset_;
  float current_scroll_delta_;

  // The height of the visible top control such that it must be shown when
  // the user stops the scroll.
  float top_controls_show_height_;

  // The height of the visible top control such that it must be hidden when
  // the user stops the scroll.
  float top_controls_hide_height_;

  DISALLOW_COPY_AND_ASSIGN(TopControlsManager);
};

}  // namespace cc

#endif  // CC_TOP_CONTROLS_MANAGER_H_
