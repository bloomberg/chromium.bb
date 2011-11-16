// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#include "base/logging.h"
#include "views/view.h"

namespace ui_controls {

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  // http://crbug.com/104396
  NOTIMPLEMENTED();
  return true;
}

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                const base::Closure& task) {
  // http://crbug.com/104396
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseMove(long x, long y) {
  // http://crbug.com/104396
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseMoveNotifyWhenDone(long x, long y, const base::Closure& task) {
  // http://crbug.com/104396
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseEvents(MouseButton type, int state) {
  // http://crbug.com/104396
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseEventsNotifyWhenDone(MouseButton type, int state,
    const base::Closure& task) {
  // http://crbug.com/104396
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseClick(MouseButton type) {
  return SendMouseEvents(type, UP | DOWN);
}

void MoveMouseToCenterAndPress(views::View* view, MouseButton button,
                               int state, const base::Closure& task) {
  // http://crbug.com/104396
  NOTIMPLEMENTED();
}

}  // namespace ui_controls
