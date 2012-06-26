// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
#define ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
#pragma once

#include "ash/shell.h"
#include "ash/touch/touch_uma.h"
#include "base/timer.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/window_observer.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/linear_animation.h"
#include "ui/gfx/point.h"

#include <map>

namespace aura {
class KeyEvent;
class LocatedEvent;
class MouseEvent;
class Window;
}

namespace ash {

namespace test {
class SystemGestureEventFilterTest;
}  // namespace test

namespace internal {

class SystemPinchHandler;
class TouchUMA;

enum BezelStart {
  BEZEL_START_UNSET = 0,
  BEZEL_START_TOP,
  BEZEL_START_LEFT,
  BEZEL_START_RIGHT,
  BEZEL_START_BOTTOM
};

enum ScrollOrientation {
  SCROLL_ORIENTATION_UNSET = 0,
  SCROLL_ORIENTATION_HORIZONTAL,
  SCROLL_ORIENTATION_VERTICAL
};

// LongPressAffordanceAnimation displays an animated affordance that is shown
// on a TAP_DOWN gesture. The animation completes on a LONG_PRESS gesture, or is
// canceled and hidden if any other event is received before that.
class LongPressAffordanceAnimation : public ui::AnimationDelegate,
                                     public ui::LinearAnimation {
 public:
  LongPressAffordanceAnimation();
  virtual ~LongPressAffordanceAnimation();

  // Display or removes long press affordance according to the |event|.
  void ProcessEvent(aura::Window* target, aura::LocatedEvent* event);

 private:
  friend class ash::test::SystemGestureEventFilterTest;

  void StartAnimation();
  void StopAnimation();

  // Overridden from ui::LinearAnimation.
  virtual void AnimateToState(double state) OVERRIDE;

  // Overridden from ui::AnimationDelegate.
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

  class LongPressAffordanceView;
  scoped_ptr<LongPressAffordanceView> view_;
  gfx::Point tap_down_location_;
  aura::Window* tap_down_target_;
  base::OneShotTimer<LongPressAffordanceAnimation> timer_;

  DISALLOW_COPY_AND_ASSIGN(LongPressAffordanceAnimation);
};

// An event filter which handles system level gesture events.
class SystemGestureEventFilter : public aura::EventFilter,
                                 public aura::WindowObserver {
 public:
  SystemGestureEventFilter();
  virtual ~SystemGestureEventFilter();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

  // Overridden from aura::WindowObserver.
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

 private:
  friend class ash::test::SystemGestureEventFilterTest;

  // Removes system-gesture handlers for a window.
  void ClearGestureHandlerForWindow(aura::Window* window);

  // Handle events meant for volume / brightness. Returns true when no further
  // events from this gesture should be sent.
  bool HandleDeviceControl(aura::Window* target, aura::GestureEvent* event);

  // Handle events meant for showing the launcher. Returns true when no further
  // events from this gesture should be sent.
  bool HandleLauncherControl(aura::GestureEvent* event);

  // Handle events meant to switch through applications. Returns true when no
  // further events from this gesture should be sent.
  bool HandleApplicationControl(aura::GestureEvent* event);

  typedef std::map<aura::Window*, SystemPinchHandler*> WindowPinchHandlerMap;
  // Created on demand when a system-level pinch gesture is initiated. Destroyed
  // when the system-level pinch gesture ends for the window.
  WindowPinchHandlerMap pinch_handlers_;

  // The percentage of the screen to the left and right which belongs to
  // device gestures.
  const int overlap_percent_;

  // Which bezel corner are we on.
  BezelStart start_location_;

  // Which orientation are we moving.
  ScrollOrientation orientation_;

  // A device swipe gesture is in progress.
  bool is_scrubbing_;

  scoped_ptr<LongPressAffordanceAnimation> long_press_affordance_;

  TouchUMA touch_uma_;

  DISALLOW_COPY_AND_ASSIGN(SystemGestureEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
