// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MONITOR_MOUSE_CURSOR_EVENT_FILTER_H
#define ASH_MONITOR_MOUSE_CURSOR_EVENT_FILTER_H
#pragma once

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/aura/event_filter.h"

namespace ash {
namespace internal {
class MonitorController;

// An event filter that controls mouse location in extended desktop
// environment.
class ASH_EXPORT MouseCursorEventFilter : public aura::EventFilter {
 public:
  MouseCursorEventFilter(MonitorController* monitor_controller);
  virtual ~MouseCursorEventFilter();

  // Overridden from aura::EventFilter:
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
  MonitorController* monitor_controller_;

  DISALLOW_COPY_AND_ASSIGN(MouseCursorEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_MONITOR_MOUSE_CURSOR_EVENT_FILTER_H
