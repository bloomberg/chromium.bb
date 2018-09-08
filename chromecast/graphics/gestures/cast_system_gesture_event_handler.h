// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_GESTURES_CAST_SYSTEM_GESTURE_EVENT_HANDLER_H_
#define CHROMECAST_GRAPHICS_GESTURES_CAST_SYSTEM_GESTURE_EVENT_HANDLER_H_

#include "base/macros.h"
#include "chromecast/graphics/gestures/cast_system_gesture_dispatcher.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}  // namespace aura

namespace chromecast {

// Observes gesture events on the root window and dispatches them to the cast
// system gesture dispatcher.
class CastSystemGestureEventHandler : public ui::EventHandler {
 public:
  explicit CastSystemGestureEventHandler(
      CastSystemGestureDispatcher* dispatcher,
      aura::Window* root_window);

  ~CastSystemGestureEventHandler() override;

  // ui::EventHandler implementation.
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  CastSystemGestureDispatcher* dispatcher_;
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(CastSystemGestureEventHandler);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_GESTURES_CAST_SYSTEM_GESTURE_EVENT_HANDLER_H_
