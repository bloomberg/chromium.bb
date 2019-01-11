// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TOUCH_UMA_H_
#define ASH_PUBLIC_CPP_TOUCH_UMA_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/gesture_action_type.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace ui {
class GestureEvent;
class TouchEvent;
}  // namespace ui

namespace ash {

// Records some touch/gesture event specific details (e.g. what gestures are
// targeted to which components etc.)
class ASH_PUBLIC_EXPORT TouchUMA {
 public:
  static void RecordGestureEvent(aura::Window* target,
                                 const ui::GestureEvent& event);
  static void RecordGestureAction(GestureActionType action);
  static void RecordTouchEvent(aura::Window* target,
                               const ui::TouchEvent& event);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TouchUMA);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TOUCH_UMA_H_
