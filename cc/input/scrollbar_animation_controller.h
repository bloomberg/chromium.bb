// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_
#define CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/input/single_scrollbar_animation_controller_thinning.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class CC_EXPORT ScrollbarAnimationControllerClient {
 public:
  virtual void PostDelayedScrollbarAnimationTask(const base::Closure& task,
                                                 base::TimeDelta delay) = 0;
  virtual void SetNeedsRedrawForScrollbarAnimation() = 0;
  virtual void SetNeedsAnimateForScrollbarAnimation() = 0;
  virtual void DidChangeScrollbarVisibility() = 0;
  virtual ScrollbarSet ScrollbarsFor(int scroll_layer_id) const = 0;

 protected:
  virtual ~ScrollbarAnimationControllerClient() {}
};

// This class fade in scrollbars when scroll and fade out after an idle delay.
// The fade animations works on both scrollbars and is controlled in this class
// This class also passes the mouse state to each
// SingleScrollbarAnimationControllerThinning. The thinning animations are
// independent between vertical/horizontal and are managed by the
// SingleScrollbarAnimationControllerThinnings.
class CC_EXPORT ScrollbarAnimationController {
 public:
  // ScrollbarAnimationController for Android. It only has fade in/out
  // animation.
  static std::unique_ptr<ScrollbarAnimationController>
  CreateScrollbarAnimationControllerAndroid(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta delay_before_starting,
      base::TimeDelta resize_delay_before_starting,
      base::TimeDelta fade_duration);

  // ScrollbarAnimationController for Desktop Overlay Scrollbar. It has fade in/
  // out animation and thinning animation.
  static std::unique_ptr<ScrollbarAnimationController>
  CreateScrollbarAnimationControllerAuraOverlay(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta delay_before_starting,
      base::TimeDelta resize_delay_before_starting,
      base::TimeDelta fade_duration,
      base::TimeDelta thinning_duration);

  ~ScrollbarAnimationController();

  bool ScrollbarsHidden() const;

  bool Animate(base::TimeTicks now);

  void DidScrollBegin();
  void DidScrollUpdate(bool on_resize);
  void DidScrollEnd();

  void DidMouseDown();
  void DidMouseUp();
  void DidMouseLeave();
  void DidMouseMoveNear(ScrollbarOrientation, float);

  bool mouse_is_over_scrollbar(ScrollbarOrientation orientation) const;
  bool mouse_is_near_scrollbar(ScrollbarOrientation orientation) const;
  bool mouse_is_near_any_scrollbar() const;

 private:
  ScrollbarAnimationController(int scroll_layer_id,
                               ScrollbarAnimationControllerClient* client,
                               base::TimeDelta delay_before_starting,
                               base::TimeDelta resize_delay_before_starting,
                               base::TimeDelta fade_duration);

  ScrollbarAnimationController(int scroll_layer_id,
                               ScrollbarAnimationControllerClient* client,
                               base::TimeDelta delay_before_starting,
                               base::TimeDelta resize_delay_before_starting,
                               base::TimeDelta fade_duration,
                               base::TimeDelta thinning_duration);

  ScrollbarSet Scrollbars() const;
  SingleScrollbarAnimationControllerThinning& GetScrollbarAnimationController(
      ScrollbarOrientation) const;

  // Returns how far through the animation we are as a progress value from
  // 0 to 1.
  float AnimationProgressAtTime(base::TimeTicks now);
  void RunAnimationFrame(float progress);

  void StartAnimation();
  void StopAnimation();

  ScrollbarAnimationControllerClient* client_;

  void PostDelayedAnimationTask(bool on_resize);

  bool Captured() const;

  void ApplyOpacityToScrollbars(float opacity);

  base::TimeTicks last_awaken_time_;
  base::TimeDelta delay_before_starting_;
  base::TimeDelta resize_delay_before_starting_;

  bool is_animating_;

  const int scroll_layer_id_;
  bool currently_scrolling_;
  bool scroll_gesture_has_scrolled_;

  base::CancelableClosure delayed_scrollbar_fade_;
  float opacity_;
  base::TimeDelta fade_duration_;

  const bool need_thinning_animation_;
  std::unique_ptr<SingleScrollbarAnimationControllerThinning>
      vertical_controller_;
  std::unique_ptr<SingleScrollbarAnimationControllerThinning>
      horizontal_controller_;

  base::WeakPtrFactory<ScrollbarAnimationController> weak_factory_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_
