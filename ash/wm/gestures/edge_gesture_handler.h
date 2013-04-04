// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_EDGE_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_EDGE_GESTURE_HANDLER_H_

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

enum EdgeScrollOrientation {
  EDGE_SCROLL_ORIENTATION_UNSET = 0,
  EDGE_SCROLL_ORIENTATION_HORIZONTAL,
  EDGE_SCROLL_ORIENTATION_VERTICAL
};

// Handles touch gestures that occur at the edge of the screen without the aid
// of a bezel sensor and might have actions associated with them.
class EdgeGestureHandler {
 public:
  EdgeGestureHandler();
  virtual ~EdgeGestureHandler();

  // Returns true of the gesture has been handled and it should not be processed
  // any farther, false otherwise.
  bool ProcessGestureEvent(aura::Window* target, const ui::GestureEvent& event);

 private:
  // Handle events meant for showing the launcher. Returns true when no further
  // events from this gesture should be sent.
  bool HandleLauncherControl(const ui::GestureEvent& event);

  bool HandleEdgeGestureStart(aura::Window* target,
                              const ui::GestureEvent& event);

  // Determine the gesture orientation (if not yet done).
  // Returns true when the orientation has been successfully determined.
  bool DetermineGestureOrientation(const ui::GestureEvent& event);

  // Handles a gesture update once the orientation has been found.
  bool HandleEdgeGestureUpdate(aura::Window* target,
                               const ui::GestureEvent& event);

  bool HandleEdgeGestureEnd(aura::Window* target,
                            const ui::GestureEvent& event);

  // Check that gesture starts near enough to the edge of the screen to be
  // handled here. If so, set |start_location_| and return true, otherwise
  // return false.
  bool GestureStartInEdgeArea(const gfx::Rect& screen,
                              const ui::GestureEvent& event);

  // Test if the gesture orientation makes sense to be dragging in or out the
  // launcher.
  bool IsGestureInLauncherOrientation(const ui::GestureEvent& event);

  // Bit flags for which edges the gesture is close to. In the case that a
  // gesture begins in a corner of the screen more then one flag may be set and
  // the orientation of the gesture will be needed to disambiguate.
  unsigned int start_location_;

  // Orientation relative to the screen that the gesture is moving in
  EdgeScrollOrientation orientation_;
  ShelfGestureHandler shelf_handler_;

  DISALLOW_COPY_AND_ASSIGN(EdgeGestureHandler);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_GESTURES_EDGE_GESTURE_HANDLER_H_
