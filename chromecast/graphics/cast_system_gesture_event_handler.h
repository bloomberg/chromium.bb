// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_SYSTEM_GESTURE_EVENT_HANDLER_H_
#define CHROMECAST_GRAPHICS_CAST_SYSTEM_GESTURE_EVENT_HANDLER_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "chromecast/graphics/cast_gesture_handler.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
}  // namespace gfx

namespace chromecast {

class SideSwipeDetector;

// Looks for root window cast system gestures, such as tap events and edge
// swipes, and dispatches them to interested observers. Installs an event
// rewriter to examine touch events for side swipe gestures. Also installs
// itself as an event handler for observing gesture tap/press events.
class CastSystemGestureEventHandler : public ui::EventHandler,
                                      public CastGestureHandler {
 public:
  explicit CastSystemGestureEventHandler(aura::Window* root_window);

  ~CastSystemGestureEventHandler() override;

  // Register a new handler for gesture events, including side swipe and tap.
  void AddGestureHandler(CastGestureHandler* handler);

  // Remove the registration of a gesture handler.
  void RemoveGestureHandler(CastGestureHandler* handler);

  // ui::EventHandler implementation.
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Implementation of CastGestureHandler methods which fan out to our gesture
  // handlers.
  bool CanHandleSwipe(CastSideSwipeOrigin swipe_origin) override;
  void HandleSideSwipeBegin(CastSideSwipeOrigin swipe_origin,
                            const gfx::Point& touch_location) override;
  void HandleSideSwipeContinue(CastSideSwipeOrigin swipe_origin,
                               const gfx::Point& touch_location) override;
  void HandleSideSwipeEnd(CastSideSwipeOrigin swipe_origin,
                          const gfx::Point& touch_location) override;

 private:
  void ProcessPressedEvent(ui::GestureEvent* event);

  aura::Window* root_window_;
  std::unique_ptr<SideSwipeDetector> side_swipe_detector_;
  base::flat_set<CastGestureHandler*> gesture_handlers_;

  DISALLOW_COPY_AND_ASSIGN(CastSystemGestureEventHandler);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_SYSTEM_GESTURE_EVENT_HANDLER_H_
