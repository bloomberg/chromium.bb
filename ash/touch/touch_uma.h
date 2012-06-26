// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_UMA_H_
#define ASH_TOUCH_TOUCH_OBSERVER_UMA_H_
#pragma once

#include <map>

#include "ash/shell.h"
#include "ui/aura/event_filter.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget.h"

namespace aura {
class GestureEvent;
class TouchEvent;
class Window;
}

namespace ash {
namespace internal {

// Records some touch/gesture event specific details (e.g. what gestures are
// targetted to which components etc.)
class TouchUMA {
 public:
  TouchUMA();
  ~TouchUMA();

  void RecordGestureEvent(aura::Window* target,
                          const aura::GestureEvent& event);
  void RecordTouchEvent(aura::Window* target,
                        const aura::TouchEvent& event);

 private:
  std::map<ui::EventType, int> ui_event_type_map_;

  DISALLOW_COPY_AND_ASSIGN(TouchUMA);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_UMA_H_
