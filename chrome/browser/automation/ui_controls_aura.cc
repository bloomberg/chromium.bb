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
  NOTIMPLEMENTED();
  return true;
}

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                Task* task) {
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseMove(long x, long y) {
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseMoveNotifyWhenDone(long x, long y, Task* task) {
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseEvents(MouseButton type, int state) {
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseEventsNotifyWhenDone(MouseButton type, int state, Task* task) {
  NOTIMPLEMENTED();
  return true;
}

bool SendMouseClick(MouseButton type) {
  return SendMouseEvents(type, UP | DOWN);
}

void MoveMouseToCenterAndPress(views::View* view, MouseButton button,
                               int state, Task* task) {
  NOTIMPLEMENTED();
}

}  // namespace ui_controls
