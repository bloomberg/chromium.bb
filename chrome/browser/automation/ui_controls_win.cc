// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "chrome/browser/automation/ui_controls_internal.h"
#include "ui/gfx/point.h"
#include "ui/views/view.h"

namespace ui_controls {

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  DCHECK(!command);  // No command key on Windows
  return internal::SendKeyPressImpl(key, control, shift, alt, base::Closure());
}

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                const base::Closure& task) {
  DCHECK(!command);  // No command key on Windows
  return internal::SendKeyPressImpl(key, control, shift, alt, task);
}

bool SendMouseMove(long x, long y) {
  return internal::SendMouseMoveImpl(x, y, base::Closure());
}

bool SendMouseMoveNotifyWhenDone(long x, long y, const base::Closure& task) {
  return internal::SendMouseMoveImpl(x, y, task);
}

bool SendMouseEvents(MouseButton type, int state) {
  return internal::SendMouseEventsImpl(type, state, base::Closure());
}

bool SendMouseEventsNotifyWhenDone(MouseButton type, int state,
                                   const base::Closure& task) {
  return internal::SendMouseEventsImpl(type, state, task);
}

bool SendMouseClick(MouseButton type) {
  return internal::SendMouseEventsImpl(type, UP | DOWN, base::Closure());
}

void MoveMouseToCenterAndPress(views::View* view,
                               MouseButton button,
                               int state,
                               const base::Closure& task) {
  DCHECK(view);
  DCHECK(view->GetWidget());
  gfx::Point view_center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &view_center);
  SendMouseMove(view_center.x(), view_center.y());
  SendMouseEventsNotifyWhenDone(button, state, task);
}

void RunClosureAfterAllPendingUIEvents(const base::Closure& closure) {
  // On windows, posting UI events is synchronous so just post the closure.
  MessageLoopForUI::current()->PostTask(FROM_HERE, closure);
}

}  // ui_controls
