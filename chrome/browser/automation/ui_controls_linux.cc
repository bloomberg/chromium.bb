// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#include "base/logging.h"

namespace ui_controls {

bool SendKeyPress(wchar_t key, bool control, bool shift, bool alt) {
  NOTIMPLEMENTED();
  return false;
}

bool SendKeyPressNotifyWhenDone(wchar_t key, bool control, bool shift,
                                bool alt, Task* task) {
  NOTIMPLEMENTED();
  return false;
}

bool SendKeyDown(wchar_t key) {
  NOTIMPLEMENTED();
  return false;
}

bool SendKeyUp(wchar_t key) {
  NOTIMPLEMENTED();
  return false;
}

bool SendMouseMove(long x, long y) {
  NOTIMPLEMENTED();
  return false;
}

void SendMouseMoveNotifyWhenDone(long x, long y, Task* task) {
  NOTIMPLEMENTED();
}

bool SendMouseClick(MouseButton type) {
  NOTIMPLEMENTED();
  return false;

}

// TODO(estade): need to figure out a better type for this than View.
void MoveMouseToCenterAndPress(views::View* view,
                               MouseButton button,
                               int state,
                               Task* task) {
  NOTIMPLEMENTED();
}

}  // namespace ui_controls
