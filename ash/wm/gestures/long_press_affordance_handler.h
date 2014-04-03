// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_LONG_PRESS_AFFORDANCE_HANDLER_H_
#define ASH_WM_GESTURES_LONG_PRESS_AFFORDANCE_HANDLER_H_

#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/point.h"

namespace ui {
class GestureEvent;
}

namespace ash {

namespace test {
class SystemGestureEventFilterTest;
}

// LongPressAffordanceHandler displays an animated affordance that is shown
// on a TAP_DOWN gesture. The animation sequence consists of two parts:
// The first part is a grow animation that starts at semi-long-press and
// completes on a long-press gesture. The affordance animates to full size
// during grow animation.
// The second part is a shrink animation that start after grow and shrinks the
// affordance out of view.
class LongPressAffordanceHandler : public gfx::LinearAnimation,
                                   public aura::WindowObserver {
 public:
  LongPressAffordanceHandler();
  virtual ~LongPressAffordanceHandler();

  // Displays or removes long press affordance according to the |event|.
  void ProcessEvent(aura::Window* target, ui::GestureEvent* event);

 private:
  friend class ash::test::SystemGestureEventFilterTest;

  class LongPressAffordanceView;

  enum LongPressAnimationType {
    NONE,
    GROW_ANIMATION,
    SHRINK_ANIMATION,
  };

  void StartAnimation();
  void StopAffordance();
  void SetTapDownTarget(aura::Window* target);

  // Overridden from gfx::LinearAnimation.
  virtual void AnimateToState(double state) OVERRIDE;
  virtual void AnimationStopped() OVERRIDE;

  // Overridden from aura::WindowObserver.
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  scoped_ptr<LongPressAffordanceView> view_;
  gfx::Point tap_down_location_;
  base::OneShotTimer<LongPressAffordanceHandler> timer_;
  aura::Window* tap_down_target_;
  LongPressAnimationType current_animation_type_;

  DISALLOW_COPY_AND_ASSIGN(LongPressAffordanceHandler);
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_LONG_PRESS_AFFORDANCE_HANDLER_H_
