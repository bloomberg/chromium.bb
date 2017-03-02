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

// This class show scrollbars when scroll and fade out after an idle delay.
// The fade animations works on both scrollbars and is controlled in this class
// This class also passes the mouse state to each
// SingleScrollbarAnimationControllerThinning. The thinning animations are
// independent between vertical/horizontal and are managed by the
// SingleScrollbarAnimationControllerThinnings.
class CC_EXPORT ScrollbarAnimationController {
 public:
  // ScrollbarAnimationController for Android. It only has show & fade out
  // animation.
  static std::unique_ptr<ScrollbarAnimationController>
  CreateScrollbarAnimationControllerAndroid(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta fade_out_delay,
      base::TimeDelta fade_out_resize_delay,
      base::TimeDelta fade_out_duration);

  // ScrollbarAnimationController for Desktop Overlay Scrollbar. It has show &
  // fade out animation and thinning animation.
  static std::unique_ptr<ScrollbarAnimationController>
  CreateScrollbarAnimationControllerAuraOverlay(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta show_delay,
      base::TimeDelta fade_out_delay,
      base::TimeDelta fade_out_resize_delay,
      base::TimeDelta fade_out_duration,
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

  static constexpr float kMouseMoveDistanceToTriggerShow = 30.0f;

 private:
  ScrollbarAnimationController(int scroll_layer_id,
                               ScrollbarAnimationControllerClient* client,
                               base::TimeDelta fade_out_delay,
                               base::TimeDelta fade_out_resize_delay,
                               base::TimeDelta fade_out_duration);

  ScrollbarAnimationController(int scroll_layer_id,
                               ScrollbarAnimationControllerClient* client,
                               base::TimeDelta show_delay,
                               base::TimeDelta fade_out_delay,
                               base::TimeDelta fade_out_resize_delay,
                               base::TimeDelta fade_out_duration,
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

  void Show();

  void PostDelayedShow();
  void PostDelayedFadeOut(bool on_resize);

  bool Captured() const;

  bool CalcNeedTriggerScrollbarShow(ScrollbarOrientation orientation,
                                    float distance) const;

  void ApplyOpacityToScrollbars(float opacity);

  ScrollbarAnimationControllerClient* client_;

  base::TimeTicks last_awaken_time_;

  // show_delay_ is only for the case where the mouse hovers near the screen
  // edge.
  base::TimeDelta show_delay_;
  base::TimeDelta fade_out_delay_;
  base::TimeDelta fade_out_resize_delay_;

  bool need_trigger_scrollbar_show_;

  bool is_animating_;

  const int scroll_layer_id_;
  bool currently_scrolling_;
  bool scroll_gesture_has_scrolled_;

  base::CancelableClosure delayed_scrollbar_show_;
  base::CancelableClosure delayed_scrollbar_fade_out_;

  float opacity_;
  base::TimeDelta fade_out_duration_;

  const bool need_thinning_animation_;
  std::unique_ptr<SingleScrollbarAnimationControllerThinning>
      vertical_controller_;
  std::unique_ptr<SingleScrollbarAnimationControllerThinning>
      horizontal_controller_;

  base::WeakPtrFactory<ScrollbarAnimationController> weak_factory_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_
