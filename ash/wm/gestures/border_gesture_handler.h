// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BORDER_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_BORDER_GESTURE_HANDLER_H_

#include <bitset>

#include "ash/shelf/shelf_types.h"
#include "ash/wm/gestures/shelf_gesture_handler.h"
#include "base/basictypes.h"

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

enum BorderScrollOrientation {
  BORDER_SCROLL_ORIENTATION_UNSET = 0,
  BORDER_SCROLL_ORIENTATION_HORIZONTAL,
  BORDER_SCROLL_ORIENTATION_VERTICAL
};

// Bit positions of border location flags
enum BorderLocation {
  BORDER_LOCATION_BOTTOM = 0,
  BORDER_LOCATION_LEFT = 1,
  BORDER_LOCATION_TOP = 2,
  BORDER_LOCATION_RIGHT = 3,
  NUM_BORDER_LOCATIONS
};

typedef std::bitset<NUM_BORDER_LOCATIONS> BorderFlags;

// Handles touch gestures that occur around the border of the display area that
// might have actions associated with them. It handles both gestures that
// require a bezel sensor (bezel gestures) and those that do not (edge
// gestures).
class BorderGestureHandler {
 public:
  BorderGestureHandler();
  ~BorderGestureHandler();

  // Returns true of the gesture has been handled and it should not be processed
  // any farther, false otherwise.
  bool ProcessGestureEvent(aura::Window* target, const ui::GestureEvent& event);

 private:
  // Handle events meant for showing the launcher. Returns true when no further
  // events from this gesture should be sent.
  bool HandleLauncherControl(const ui::GestureEvent& event);

  bool HandleBorderGestureStart(aura::Window* target,
                                const ui::GestureEvent& event);

  // Handles a gesture update once the orientation has been found.
  bool HandleBorderGestureUpdate(aura::Window* target,
                                 const ui::GestureEvent& event);

  bool HandleBorderGestureEnd(aura::Window* target,
                              const ui::GestureEvent& event);

  // Check that gesture starts on a bezel or in the edge region of the
  // screen. If so, set bits in |start_location_|.
  void GestureStartInTargetArea(const gfx::Rect& screen,
                                const ui::GestureEvent& event);

  // Determine the gesture orientation (if not yet done).
  // Returns true when the orientation has been successfully determined.
  bool DetermineGestureOrientation(const ui::GestureEvent& event);

  // Test if the gesture orientation makes sense to be dragging in or out the
  // launcher.
  bool IsGestureInLauncherOrientation(const ui::GestureEvent& event);

  // Which bezel/edges the gesture started in. In the case that a gesture begins
  // in a corner of the screen more then one flag may be set and the orientation
  // of the gesture will be needed to disambiguate.
  BorderFlags start_location_;

  // Orientation relative to the screen that the gesture is moving in
  BorderScrollOrientation orientation_;

  ShelfGestureHandler shelf_handler_;

  DISALLOW_COPY_AND_ASSIGN(BorderGestureHandler);
};

}  // namespace internal
}  // namespace ash
#endif  // ASH_WM_GESTURES_BORDER_GESTURE_HANDLER_H_
