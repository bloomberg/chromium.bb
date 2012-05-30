// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
#define ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
#pragma once

#include "ash/shell.h"
#include "ui/aura/event_filter.h"

namespace aura {
class MouseEvent;
class KeyEvent;
class Window;
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

// An event filter which handles system level gesture events.
class SystemGestureEventFilter : public aura::EventFilter {
 public:
  SystemGestureEventFilter();
  virtual ~SystemGestureEventFilter();

  // Overriden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  // Handle events meant for volume / brightness. Returns true when no further
  // events from this gesture should be sent.
  bool HandleDeviceControl(aura::GestureEvent* event);

  // Handle events meant for showing the launcher. Returns true when no further
  // events from this gesture should be sent.
  bool HandleLauncherControl(aura::GestureEvent* event);

  // Handle events meant to switch through applications. Returns true when no
  // further events from this gesture should be sent.
  bool HandleApplicationControl(aura::GestureEvent* event);

  // The percentage of the screen to the left and right which belongs to
  // device gestures.
  const int overlap_percent_;

  // Which bezel corner are we on.
  BezelStart start_location_;

  // Which orientation are we moving.
  ScrollOrientation orientation_;

  // A device swipe gesture is in progress.
  bool is_scrubbing_;

  DISALLOW_COPY_AND_ASSIGN(SystemGestureEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
