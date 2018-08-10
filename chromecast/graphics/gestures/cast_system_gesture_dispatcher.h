// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_GESTURES_CAST_SYSTEM_GESTURE_DISPATCHER_H_
#define CHROMECAST_GRAPHICS_GESTURES_CAST_SYSTEM_GESTURE_DISPATCHER_H_

#include "base/containers/flat_set.h"
#include "chromecast/graphics/cast_gesture_handler.h"

namespace chromecast {

// A CastGestureHandler that fans system gesture events out to multiple
// consumers.
class CastSystemGestureDispatcher : public CastGestureHandler {
 public:
  CastSystemGestureDispatcher();

  ~CastSystemGestureDispatcher() override;

  // Register a new handler for gesture events, including side swipe and tap.
  void AddGestureHandler(CastGestureHandler* handler);

  // Remove the registration of a gesture handler.
  void RemoveGestureHandler(CastGestureHandler* handler);

  // Implementation of CastGestureHandler methods which fan out to our gesture
  // handlers.
  bool CanHandleSwipe(CastSideSwipeOrigin swipe_origin) override;
  void HandleSideSwipeBegin(CastSideSwipeOrigin swipe_origin,
                            const gfx::Point& touch_location) override;
  void HandleSideSwipeContinue(CastSideSwipeOrigin swipe_origin,
                               const gfx::Point& touch_location) override;
  void HandleSideSwipeEnd(CastSideSwipeOrigin swipe_origin,
                          const gfx::Point& touch_location) override;
  void HandleTapDownGesture(const gfx::Point& touch_location) override;
  void HandleTapGesture(const gfx::Point& touch_location) override;
  void HandleScreenExit(CastSideSwipeOrigin side,
                        const gfx::Point& touch_location) override;
  void HandleScreenEnter(CastSideSwipeOrigin side,
                         const gfx::Point& touch_location) override;
  Corner HandledCornerHolds() const override;
  void HandleCornerHold(Corner corner_origin,
                        const ui::TouchEvent& touch_event) override;
  void HandleCornerHoldEnd(Corner corner_origin,
                           const ui::TouchEvent& touch_event) override;

 private:
  base::flat_set<CastGestureHandler*> gesture_handlers_;
};
}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_GESTURES_CAST_SYSTEM_GESTURE_DISPATCHER_H_