// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_UI_CONTROLS_INTERNAL_H_
#define CHROME_BROWSER_AUTOMATION_UI_CONTROLS_INTERNAL_H_

#include "base/callback_forward.h"
#include "chrome/browser/automation/ui_controls.h"

namespace ui_controls {
namespace internal {

// A utility function to send a mouse click event in a closure. It's shared by
// ui_controls_linux.cc and ui_controls_mac.cc
void ClickTask(MouseButton button, int state, const base::Closure& followup);

#if defined(OS_WIN)
// A utility functions for windows to send key or mouse events and
// run the task.
bool SendKeyPressImpl(ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      const base::Closure& task);
bool SendMouseMoveImpl(long x, long y, const base::Closure& task);
bool SendMouseEventsImpl(MouseButton type,
                         int state,
                         const base::Closure& task);
#endif

}  // namespace internal
}  // namespace ui_controls

#endif  // CHROME_BROWSER_AUTOMATION_UI_CONTROLS_INTERNAL_H_
