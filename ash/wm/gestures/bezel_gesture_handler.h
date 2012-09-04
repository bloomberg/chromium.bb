// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BEZEL_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_BEZEL_GESTURE_HANDLER_H_

#include "base/basictypes.h"

#include "ash/wm/gestures/shelf_gesture_handler.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
class Rect;
}

namespace ui {
class GestureEvent;
}

namespace ash {
namespace internal {

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

class BezelGestureHandler {
 public:
  BezelGestureHandler();
  virtual ~BezelGestureHandler();

  void ProcessGestureEvent(aura::Window* target, const ui::GestureEvent& event);

 private:
  // Handle events meant for volume / brightness. Returns true when no further
  // events from this gesture should be sent.
  bool HandleDeviceControl(const gfx::Rect& screen,
                           const ui::GestureEvent& event);

  // Handle events meant for showing the launcher. Returns true when no further
  // events from this gesture should be sent.
  bool HandleLauncherControl(const ui::GestureEvent& event);

  // Handle events meant to switch through applications. Returns true when no
  // further events from this gesture should be sent.
  bool HandleApplicationControl(const ui::GestureEvent& event);

  // Handle a gesture begin event.
  void HandleBezelGestureStart(aura::Window* target,
                               const ui::GestureEvent& event);

  // Determine the gesture orientation (if not yet done).
  // Returns true when the orientation has been successfully determined.
  bool DetermineGestureOrientation(const ui::GestureEvent& event);

  // Handles a gesture update once the orientation has been found.
  void HandleBezelGestureUpdate(aura::Window* target,
                                const ui::GestureEvent& event);

  // End a bezel gesture.
  void HandleBezelGestureEnd();

  // Check if a bezel slider gesture is still within the bezel area.
  // If it is not, it will abort the gesture, otherwise it will return true.
  bool GestureInBezelArea(const gfx::Rect& screen,
                          const ui::GestureEvent& event);

  // Check if the bezel gesture for noise artifacts. If it is no noise
  // it will return false. If noise is detected it will abort the gesture.
  bool BezelGestureMightBeNoise(const gfx::Rect& screen,
                                const ui::GestureEvent& event);

  // Most noise events are showing up at the upper and lower extremes. To
  // filter them out, a few percent get cut off at the top and at the bottom.
  // A return value of false indicates possible noise and no further action
  // should be performed with the event.
  // The returned |percent| value is between 0.0 and 100.0.
  bool DeNoiseBezelSliderPosition(double* percent);

  // Determine the brightness value from the slider position.
  double LimitBezelBrightnessFromSlider(double percent);

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

  // True if device control bezel operations (brightness, volume) are enabled.
  bool enable_bezel_device_control_;

  ShelfGestureHandler shelf_handler_;

  DISALLOW_COPY_AND_ASSIGN(BezelGestureHandler);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_GESTURES_BEZEL_GESTURE_HANDLER_H_
