// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_AUTOCLICK_RING_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_UI_AUTOCLICK_RING_HANDLER_H_

#include <memory>

#include "ash/autoclick/autoclick_controller.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"

namespace chromeos {

// AutoclickRingHandler displays an animated affordance that is shown
// on autoclick gesture. The animation sequence consists of two circles which
// shrink towards the spot the autoclick will generate a mouse event.
class AutoclickRingHandler : public gfx::LinearAnimation,
                             public aura::WindowObserver,
                             public ash::AutoclickController::Delegate {
 public:
  AutoclickRingHandler();
  ~AutoclickRingHandler() override;

 private:
  // Overriden from ash::AutoclickController::Delegate.
  void StartGesture(base::TimeDelta duration,
                    const gfx::Point& center_point_in_screen) override;
  void StopGesture() override;
  void SetGestureCenter(const gfx::Point& center_point_in_screen) override;

  class AutoclickRingView;

  enum class AnimationType {
    NONE,
    GROW_ANIMATION,
    SHRINK_ANIMATION,
  };

  aura::Window* GetTargetWindow();
  void SetTapDownTarget();
  void StartAnimation(base::TimeDelta duration);
  void StopAutoclickRing();
  void SetTapDownTarget(aura::Window* target);

  // Overridden from gfx::LinearAnimation.
  void AnimateToState(double state) override;
  void AnimationStopped() override;

  // Overridden from aura::WindowObserver.
  void OnWindowDestroying(aura::Window* window) override;

  std::unique_ptr<AutoclickRingView> view_;
  // Location of the simulated mouse event from auto click in screen
  // coordinates.
  gfx::Point tap_down_location_;
  // The target window is observed by AutoclickRingHandler for the duration of
  // a autoclick gesture.
  aura::Window* tap_down_target_;
  AnimationType current_animation_type_;
  base::TimeDelta animation_duration_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickRingHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_AUTOCLICK_RING_HANDLER_H_
