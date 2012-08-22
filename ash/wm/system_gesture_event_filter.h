// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
#define ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_

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
class Window;
}

namespace ui {
class LocatedEvent;
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
// on a TAP_DOWN gesture. The animation sequence consists of two parts:
// The first part is a grow animation that starts at semi-long-press and
// completes on a long-press gesture. The affordance animates to full size
// during grow animation.
// The second part is a shrink animation that start after grow and shrinks the
// affordance out of view.
class LongPressAffordanceAnimation : public ui::AnimationDelegate,
                                     public ui::LinearAnimation {
 public:
  LongPressAffordanceAnimation();
  virtual ~LongPressAffordanceAnimation();

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
  base::OneShotTimer<LongPressAffordanceAnimation> timer_;
  int tap_down_display_id_;
  LongPressAnimationType current_animation_type_;

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
                                 ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

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
  bool HandleDeviceControl(const gfx::Rect& screen, ui::GestureEvent* event);

  // Handle events meant for showing the launcher. Returns true when no further
  // events from this gesture should be sent.
  bool HandleLauncherControl(ui::GestureEvent* event);

  // Handle events meant to switch through applications. Returns true when no
  // further events from this gesture should be sent.
  bool HandleApplicationControl(ui::GestureEvent* event);

  // Handle a gesture begin event.
  void HandleBezelGestureStart(aura::Window* target, ui::GestureEvent* event);

  // Determine the gesture orientation (if not yet done).
  // Returns true when the orientation has been successfully determined.
  bool DetermineGestureOrientation(ui::GestureEvent* event);

  // Handles a gesture update once the orientation has been found.
  void HandleBezelGestureUpdate(aura::Window* target, ui::GestureEvent* event);

  // End a bezel gesture.
  void HandleBezelGestureEnd();

  // Check if a bezel slider gesture is still within the bezel area.
  // If it is not, it will abort the gesture, otherwise it will return true.
  bool GestureInBezelArea(const gfx::Rect& screen, ui::GestureEvent* event);

  // Check if the bezel gesture for noise artifacts. If it is no noise
  // it will return false. If noise is detected it will abort the gesture.
  bool BezelGestureMightBeNoise(const gfx::Rect& screen,
                                ui::GestureEvent* event);

  // Most noise events are showing up at the upper and lower extremes. To
  // filter them out, a few percent get cut off at the top and at the bottom.
  // A return value of false indicates possible noise and no further action
  // should be performed with the event.
  // The returned |percent| value is between 0.0 and 100.0.
  bool DeNoiseBezelSliderPosition(double& percent);

  // Determine the brightness value from the slider position.
  double LimitBezelBrightnessFromSlider(double percent);

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

  // To suppress random noise in the bezel area, the stroke needs to have at
  // least a certain amount of events in close proximity before it gets used.
  // This is the counter which keeps track of the number of events passed.
  int initiation_delay_events_;

  scoped_ptr<LongPressAffordanceAnimation> long_press_affordance_;

  TouchUMA touch_uma_;

  DISALLOW_COPY_AND_ASSIGN(SystemGestureEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
