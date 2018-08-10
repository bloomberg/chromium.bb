// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_GESTURE_HANDLER_H_
#define CHROMECAST_GRAPHICS_CAST_GESTURE_HANDLER_H_

#include "ui/events/event.h"

// TODO(rdaum): Move into chromecast/graphics/gestures, which will require some
// cross-repo maneuvers.
#include "base/macros.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace chromecast {

enum class CastSideSwipeOrigin { TOP, BOTTOM, LEFT, RIGHT, NONE };

// Interface for handlers triggered on reception of gestures on the
// root window, including side-swipe and tap.
class CastGestureHandler {
 public:
  CastGestureHandler() = default;
  virtual ~CastGestureHandler() = default;

  // Return true if this handler can handle swipes from the given origin.
  virtual bool CanHandleSwipe(CastSideSwipeOrigin swipe_origin);

  // Triggered on the beginning of a swipe from edge.
  virtual void HandleSideSwipeBegin(CastSideSwipeOrigin swipe_origin,
                                    const gfx::Point& touch_location) {}

  // Triggered on the continuance (finger drag) of a swipe from edge.
  virtual void HandleSideSwipeContinue(CastSideSwipeOrigin swipe_origin,
                                       const gfx::Point& touch_location) {}

  // Triggered on the completion (finger up) of a swipe from edge.
  virtual void HandleSideSwipeEnd(CastSideSwipeOrigin swipe_origin,
                                  const gfx::Point& touch_location) {}

  // Triggered on the completion of a tap down event, fired when the
  // finger is pressed.
  virtual void HandleTapDownGesture(const gfx::Point& touch_location) {}

  // Triggered on the completion of a tap event, fire after a press
  // followed by a release, within the tap timeout window
  virtual void HandleTapGesture(const gfx::Point& touch_location) {}

  // Triggered when the finger enters a side margin from inside the screen.
  // That is, the finger is leaving the screen.
  virtual void HandleScreenExit(CastSideSwipeOrigin side,
                                const gfx::Point& touch_location) {}

  // Triggered when the finger enters a side margin from outside the screen.
  // That is, the finger is entering the screen.
  virtual void HandleScreenEnter(CastSideSwipeOrigin side,
                                 const gfx::Point& touch_location) {}

  enum Corner {
    NO_CORNERS = 0,
    TOP_LEFT_CORNER = 1 << 0,
    BOTTOM_LEFT_CORNER = 1 << 1,
    TOP_RIGHT_CORNER = 1 << 2,
    BOTTOM_RIGHT_CORNER = 1 << 3,
  };

  // Return a bitmask of the corners that this handler is interested in, if any.
  virtual Corner HandledCornerHolds() const;

  // Triggered when the finger has been held inside the corner for longer
  // than the corner hold threshold time.
  virtual void HandleCornerHold(Corner corner_origin,
                                const ui::TouchEvent& touch_event) {}

  // Triggered when a corner hold is ended because the finger has left the
  // corner or has been released.
  virtual void HandleCornerHoldEnd(Corner corner_origin,
                                   const ui::TouchEvent& touch_event) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CastGestureHandler);
};

}  // namespace chromecast

#endif  //  CHROMECAST_GRAPHICS_CAST_GESTURE_HANDLER_H_
