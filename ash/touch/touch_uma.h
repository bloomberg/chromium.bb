// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_UMA_H_
#define ASH_TOUCH_TOUCH_OBSERVER_UMA_H_

#include <map>

#include "ash/shell.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget.h"

namespace aura {
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
                          const ui::GestureEvent& event);
  void RecordTouchEvent(aura::Window* target,
                        const ui::TouchEvent& event);

 private:
  void UpdateBurstData(const ui::TouchEvent& event);

  // These are used to measure the number of touch-start events we receive in a
  // quick succession, regardless of the target window.
  bool touch_in_progress_;
  int burst_length_;
  base::TimeDelta last_touch_down_time_;

  DISALLOW_COPY_AND_ASSIGN(TouchUMA);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_UMA_H_
