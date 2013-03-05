// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"

namespace ui_test_utils {

namespace {

bool GetNativeWindow(const Browser* browser, gfx::NativeWindow* native_window) {
  BrowserWindow* window = browser->window();
  if (!window)
    return false;

  *native_window = window->GetNativeWindow();
  return *native_window;
}

}  // namespace

bool BringBrowserWindowToFront(const Browser* browser) {
  gfx::NativeWindow window = NULL;
  if (!GetNativeWindow(browser, &window))
    return false;

  return ui_test_utils::ShowAndFocusNativeWindow(window);
}

bool SendKeyPressSync(const Browser* browser,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) {
  gfx::NativeWindow window = NULL;
  if (!GetNativeWindow(browser, &window))
    return false;
  return SendKeyPressToWindowSync(window, key, control, shift, alt, command);
}

bool SendKeyPressToWindowSync(const gfx::NativeWindow window,
                              ui::KeyboardCode key,
                              bool control,
                              bool shift,
                              bool alt,
                              bool command) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  bool result;
  result = ui_controls::SendKeyPressNotifyWhenDone(
      window, key, control, shift, alt, command, runner->QuitClosure());
#if defined(OS_WIN)
  if (!result && ui_test_utils::ShowAndFocusNativeWindow(window)) {
    result = ui_controls::SendKeyPressNotifyWhenDone(
        window, key, control, shift, alt, command, runner->QuitClosure());
  }
#endif
  if (!result) {
    LOG(ERROR) << "ui_controls::SendKeyPressNotifyWhenDone failed";
    return false;
  }

  // Run the message loop. It'll stop running when either the key was received
  // or the test timed out (in which case testing::Test::HasFatalFailure should
  // be set).
  runner->Run();
  return !testing::Test::HasFatalFailure();
}

bool SendKeyPressAndWait(const Browser* browser,
                         ui::KeyboardCode key,
                         bool control,
                         bool shift,
                         bool alt,
                         bool command,
                         int type,
                         const content::NotificationSource& source) {
  content::WindowedNotificationObserver observer(type, source);

  if (!SendKeyPressSync(browser, key, control, shift, alt, command))
    return false;

  observer.Wait();
  return !testing::Test::HasFatalFailure();
}

bool SendMouseMoveSync(const gfx::Point& location) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  if (!ui_controls::SendMouseMoveNotifyWhenDone(
          location.x(), location.y(), runner->QuitClosure())) {
    return false;
  }
  runner->Run();
  return !testing::Test::HasFatalFailure();
}

bool SendMouseEventsSync(ui_controls::MouseButton type, int state) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  if (!ui_controls::SendMouseEventsNotifyWhenDone(
          type, state, runner->QuitClosure())) {
    return false;
  }
  runner->Run();
  return !testing::Test::HasFatalFailure();
}

namespace internal {

void ClickTask(ui_controls::MouseButton button,
               int state,
               const base::Closure& followup) {
  if (!followup.is_null())
    ui_controls::SendMouseEventsNotifyWhenDone(button, state, followup);
  else
    ui_controls::SendMouseEvents(button, state);
}

}  // namespace internal

}  // namespace ui_test_utils
