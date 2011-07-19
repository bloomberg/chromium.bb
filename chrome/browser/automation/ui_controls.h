// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H_
#define CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include <wtypes.h>
#endif

#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

#if defined(TOOLKIT_VIEWS)
namespace views {
class View;
}
#endif

class Task;

namespace ui_controls {

// Many of the functions in this class include a variant that takes a Task.
// The version that takes a Task waits until the generated event is processed.
// Once the generated event is processed the Task is Run (and deleted). Note
// that this is a somewhat fragile process in that any event of the correct
// type (key down, mouse click, etc.) will trigger the Task to be run. Hence
// a usage such as
//
//   SendKeyPress(...);
//   SendKeyPressNotifyWhenDone(..., task);
//
// might trigger |task| early.
//
// Note: Windows does not currently do anything with the |window| argument for
// these functions, so passing NULL is ok.

// Send a key press with/without modifier keys.
//
// If you're writing a test chances are you want the variant in ui_test_utils.
// See it for details.
bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command);
bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                Task* task);

// Simulate a mouse move. (x,y) are absolute screen coordinates.
bool SendMouseMove(long x, long y);
bool SendMouseMoveNotifyWhenDone(long x, long y, Task* task);

enum MouseButton {
  LEFT = 0,
  MIDDLE,
  RIGHT,
};

// Used to indicate the state of the button when generating events.
enum MouseButtonState {
  UP = 1,
  DOWN = 2
};

// Sends a mouse down and/or up message. The click will be sent to wherever
// the cursor currently is, so be sure to move the cursor before calling this
// (and be sure the cursor has arrived!).
bool SendMouseEvents(MouseButton type, int state);
bool SendMouseEventsNotifyWhenDone(MouseButton type, int state, Task* task);
// Same as SendMouseEvents with UP | DOWN.
bool SendMouseClick(MouseButton type);

// A combination of SendMouseMove to the middle of the view followed by
// SendMouseEvents.
void MoveMouseToCenterAndPress(
#if defined(TOOLKIT_VIEWS)
    views::View* view,
#elif defined(TOOLKIT_GTK)
    GtkWidget* widget,
#elif defined(OS_MACOSX)
    NSView* view,
#endif
    MouseButton button,
    int state,
    Task* task);

}  // ui_controls

#endif  // CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H_
