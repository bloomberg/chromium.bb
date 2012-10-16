// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_LONG_PRESS_AFFORDANCE_HANDLER_H_
#define ASH_WM_GESTURES_LONG_PRESS_AFFORDANCE_HANDLER_H_

#include "base/timer.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/linear_animation.h"
#include "ui/gfx/point.h"

namespace aura {
class Window;
}

namespace ui {
class LocatedEvent;
}

namespace ash {

namespace test {
class SystemGestureEventFilterTest;
}

namespace internal {

// LongPressAffordanceHandler displays an animated affordance that is shown
// on a TAP_DOWN gesture. The animation sequence consists of two parts:
// The first part is a grow animation that starts at semi-long-press and
// completes on a long-press gesture. The affordance animates to full size
// during grow animation.
// The second part is a shrink animation that start after grow and shrinks the
// affordance out of view.
class LongPressAffordanceHandler : public ui::AnimationDelegate,
                                   public ui::LinearAnimation {
 public:
  LongPressAffordanceHandler();
  virtual ~LongPressAffordanceHandler();

  // Display or removes long press affordance according to the |event|.
  void ProcessEvent(aura::Window* target,
                    ui::LocatedEvent* event,
                    int touch_id);

 private:
  friend class ash::test::SystemGestureEventFilterTest;

  enum LongPressAnimationType {
    NONE,
    GROW_ANIMATION,
    SHRINK_ANIMATION,
  };

  void StartAnimation();
  void StopAnimation();

  // Overridden from ui::LinearAnimation.
  virtual void AnimateToState(double state) OVERRIDE;
  virtual bool ShouldSendCanceledFromStop() OVERRIDE;

  // Overridden from ui::AnimationDelegate.
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  class LongPressAffordanceView;
  scoped_ptr<LongPressAffordanceView> view_;
  gfx::Point tap_down_location_;
  int tap_down_touch_id_;
  base::OneShotTimer<LongPressAffordanceHandler> timer_;
  int64 tap_down_display_id_;
  LongPressAnimationType current_animation_type_;

  DISALLOW_COPY_AND_ASSIGN(LongPressAffordanceHandler);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_GESTURES_LONG_PRESS_AFFORDANCE_HANDLER_H_
