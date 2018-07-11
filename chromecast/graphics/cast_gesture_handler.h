// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_GESTURE_HANDLER_H_
#define CHROMECAST_GRAPHICS_CAST_GESTURE_HANDLER_H_

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

 private:
  DISALLOW_COPY_AND_ASSIGN(CastGestureHandler);
};

}  // namespace chromecast

#endif  //  CHROMECAST_GRAPHICS_CAST_GESTURE_HANDLER_H_
