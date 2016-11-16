// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_INTERACTIVE_TEST_UTILS_H_
#define CHROME_TEST_BASE_INTERACTIVE_TEST_UTILS_H_

#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/test/ui_controls.h"

namespace gfx {
class Point;
}

#if defined(TOOLKIT_VIEWS)
namespace views {
class View;
}
#endif

namespace ui_test_utils {

// Brings the native window for |browser| to the foreground and waits until the
// browser is active.
bool BringBrowserWindowToFront(const Browser* browser) WARN_UNUSED_RESULT;

// Returns true if the View is focused.
bool IsViewFocused(const Browser* browser, ViewID vid);

// Simulates a mouse click on a View in the browser.
void ClickOnView(const Browser* browser, ViewID vid);

// Makes focus shift to the given View without clicking it.
void FocusView(const Browser* browser, ViewID vid);

// A collection of utilities that are used from interactive_ui_tests. These are
// separated from ui_test_utils.h to ensure that browser_tests don't use them,
// since they depend on focus which isn't possible for sharded test.

// Hide a native window.
void HideNativeWindow(gfx::NativeWindow window);

// Show and focus a native window. Returns true on success.
bool ShowAndFocusNativeWindow(gfx::NativeWindow window) WARN_UNUSED_RESULT;

// Sends a key press, blocking until the key press is received or the test times
// out. This uses ui_controls::SendKeyPress, see it for details. Returns true
// if the event was successfully sent and received.
bool SendKeyPressSync(const Browser* browser,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) WARN_UNUSED_RESULT;

// Sends a key press, blocking until the key press is received or the test times
// out. This uses ui_controls::SendKeyPress, see it for details. Returns true
// if the event was successfully sent and received.
bool SendKeyPressToWindowSync(const gfx::NativeWindow window,
                              ui::KeyboardCode key,
                              bool control,
                              bool shift,
                              bool alt,
                              bool command) WARN_UNUSED_RESULT;

// Sends a key press, blocking until both the key press and a notification from
// |source| of type |type| are received, or until the test times out. This uses
// ui_controls::SendKeyPress, see it for details. Returns true if the event was
// successfully sent and both the event and notification were received.
bool SendKeyPressAndWait(const Browser* browser,
                         ui::KeyboardCode key,
                         bool control,
                         bool shift,
                         bool alt,
                         bool command,
                         int type,
                         const content::NotificationSource& source)
                             WARN_UNUSED_RESULT;

// Sends a move event blocking until received. Returns true if the event was
// successfully received. This uses ui_controls::SendMouse***NotifyWhenDone,
// see it for details.
bool SendMouseMoveSync(const gfx::Point& location) WARN_UNUSED_RESULT;
bool SendMouseEventsSync(ui_controls::MouseButton type,
                         int state) WARN_UNUSED_RESULT;

// See SendKeyPressAndWait.  This function additionally performs a check on the
// NotificationDetails using the provided Details<U>.
template <class U>
bool SendKeyPressAndWaitWithDetails(
    const Browser* browser,
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command,
    int type,
    const content::NotificationSource& source,
    const content::Details<U>& details) WARN_UNUSED_RESULT;

template <class U>
bool SendKeyPressAndWaitWithDetails(
    const Browser* browser,
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command,
    int type,
    const content::NotificationSource& source,
    const content::Details<U>& details) {
  WindowedNotificationObserverWithDetails<U> observer(type, source);

  if (!SendKeyPressSync(browser, key, control, shift, alt, command))
    return false;

  observer.Wait();

  U my_details;
  if (!observer.GetDetailsFor(source.map_key(), &my_details))
    return false;

  return *details.ptr() == my_details && !testing::Test::HasFatalFailure();
}

// A combination of SendMouseMove to the middle of the view followed by
// SendMouseEvents. Only exposed for toolkit-views.
// Alternatives: ClickOnView() and ui::test::EventGenerator.
#if defined(TOOLKIT_VIEWS)
void MoveMouseToCenterAndPress(views::View* view,
                               ui_controls::MouseButton button,
                               int state,
                               const base::Closure& task);

// Returns the center of |view| in screen coordinates.
gfx::Point GetCenterInScreenCoordinates(const views::View* view);
#endif

namespace internal {

// A utility function to send a mouse click event in a closure. It's shared by
// ui_controls_linux.cc and ui_controls_mac.cc
void ClickTask(ui_controls::MouseButton button,
               int state,
               const base::Closure& followup);

}  // namespace internal

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_INTERACTIVE_TEST_UTILS_H_
