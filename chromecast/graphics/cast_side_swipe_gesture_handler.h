// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_SIDE_SWIPE_GESTURE_HANDLER_H_
#define CHROMECAST_GRAPHICS_CAST_SIDE_SWIPE_GESTURE_HANDLER_H_

#include "base/macros.h"

namespace ui {
class GestureEvent;
}  // namespace ui

namespace chromecast {

enum class CastSideSwipeOrigin { TOP, BOTTOM, LEFT, RIGHT, NONE };

// Interface for handlers triggered on reception of side-swipe gestures on the
// root window.
class CastSideSwipeGestureHandlerInterface {
 public:
  CastSideSwipeGestureHandlerInterface() = default;
  virtual ~CastSideSwipeGestureHandlerInterface() = default;

  // Triggered on the beginning of a swipe.
  // Note: Consumers of the event should call SetHandled on it to prevent its
  // further propagation.
  virtual void OnSideSwipeBegin(CastSideSwipeOrigin swipe_origin,
                                ui::GestureEvent* gesture_event) = 0;

  // Triggered on the completion (finger up) of a swipe.
  virtual void OnSideSwipeEnd(CastSideSwipeOrigin swipe_origin,
                              ui::GestureEvent* gesture_event) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastSideSwipeGestureHandlerInterface);
};

}  // namespace chromecast

#endif  //  CHROMECAST_GRAPHICS_CAST_SIDE_SWIPE_GESTURE_HANDLER_H_
