// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/ui/native_app_window.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/test/base/interactive_test_utils.h"

using namespace apps;

// Helper class that has to be created in the stack to check if the fullscreen
// setting of a NativeWindow has changed since the creation of the object.
class FullscreenChangeWaiter {
 public:
  explicit FullscreenChangeWaiter(NativeAppWindow* window)
      : window_(window),
        initial_fullscreen_state_(window_->IsFullscreen()) {}

  void Wait() {
    while (initial_fullscreen_state_ != window_->IsFullscreen())
      content::RunAllPendingInMessageLoop();
  }

 private:
  NativeAppWindow* window_;
  bool initial_fullscreen_state_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenChangeWaiter);
};

class AppWindowInteractiveTest : public extensions::PlatformAppBrowserTest {
 public:
  bool RunAppWindowInteractiveTest(const char* testName) {
    ExtensionTestMessageListener launched_listener("Launched", true);
    LoadAndLaunchPlatformApp("window_api_interactive");
    if (!launched_listener.WaitUntilSatisfied()) {
      message_ = "Did not get the 'Launched' message.";
      return false;
    }

    ResultCatcher catcher;
    launched_listener.Reply(testName);

    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }

    return true;
  }

  bool SimulateKeyPress(ui::KeyboardCode key) {
    return ui_test_utils::SendKeyPressToWindowSync(
      GetFirstShellWindow()->GetNativeWindow(),
      key,
      false,
      false,
      false,
      false);
  }
};

#if defined(OS_LINUX) && defined(USE_AURA)
// These tests do not work on Linux Aura because when the window is raised, the
// content is not focused thus do not get the key events.
// See http://crbug.com/324346
#define MAYBE_ESCDoesNotLeaveFullscreenWindow \
  DISABLED_ESCDoesNotLeaveFullscreenWindow
#define MAYBE_ESCDoesNotLeaveFullscreenDOM DISABLED_ESCDoesNotLeaveFullscreenDOM
// These tests are failing on Linux Aura for unknown reasons.
#define MAYBE_ESCLeavesFullscreenWindow DISABLED_ESCLeavesFullscreenWindow
#define MAYBE_ESCLeavesFullscreenDOM DISABLED_ESCLeavesFullscreenDOM
#elif defined(OS_MACOSX)
// These tests are highly flaky on MacOS.
#define MAYBE_ESCLeavesFullscreenWindow DISABLED_ESCLeavesFullscreenWindow
#define MAYBE_ESCLeavesFullscreenDOM DISABLED_ESCLeavesFullscreenDOM
#define MAYBE_ESCDoesNotLeaveFullscreenWindow \
  DISABLED_ESCDoesNotLeaveFullscreenWindow
#define MAYBE_ESCDoesNotLeaveFullscreenDOM DISABLED_ESCDoesNotLeaveFullscreenDOM
#else
#define MAYBE_ESCLeavesFullscreenWindow ESCLeavesFullscreenWindow
#define MAYBE_ESCLeavesFullscreenDOM ESCLeavesFullscreenDOM
#define MAYBE_ESCDoesNotLeaveFullscreenWindow ESCDoesNotLeaveFullscreenWindow
#define MAYBE_ESCDoesNotLeaveFullscreenDOM ESCDoesNotLeaveFullscreenDOM
#endif

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest,
                       MAYBE_ESCLeavesFullscreenWindow) {
  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("leave_fullscreen");
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // When receiving the reply, the application will try to go fullscreen using
  // the Window API but there is no synchronous way to know if that actually
  // succeeded. Also, failure will not be notified. A failure case will only be
  // known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstShellWindow()->GetBaseWindow());

    launched_listener.Reply("window");

    fs_changed.Wait();
  }

  // Same idea as above but for leaving fullscreen. Fullscreen mode should be
  // left when ESC is received.
  {
    FullscreenChangeWaiter fs_changed(GetFirstShellWindow()->GetBaseWindow());

    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_ESCAPE));

    fs_changed.Wait();
  }
}

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest, MAYBE_ESCLeavesFullscreenDOM) {
  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("leave_fullscreen");
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  launched_listener.Reply("dom");

  // Because the DOM way to go fullscreen requires user gesture, we simulate a
  // key event to get the window entering in fullscreen mode. The reply will
  // make the window listen for the key event. The reply will be sent to the
  // renderer process before the keypress and should be received in that order.
  // When receiving the key event, the application will try to go fullscreen
  // using the Window API but there is no synchronous way to know if that
  // actually succeeded. Also, failure will not be notified. A failure case will
  // only be known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstShellWindow()->GetBaseWindow());

    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_A));

    fs_changed.Wait();
  }

  // Same idea as above but for leaving fullscreen. Fullscreen mode should be
  // left when ESC is received.
  {
    FullscreenChangeWaiter fs_changed(GetFirstShellWindow()->GetBaseWindow());

    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_ESCAPE));

    fs_changed.Wait();
  }
}

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest,
                       MAYBE_ESCDoesNotLeaveFullscreenWindow) {
  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("prevent_leave_fullscreen");
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // When receiving the reply, the application will try to go fullscreen using
  // the Window API but there is no synchronous way to know if that actually
  // succeeded. Also, failure will not be notified. A failure case will only be
  // known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstShellWindow()->GetBaseWindow());

    launched_listener.Reply("window");

    fs_changed.Wait();
  }

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_ESCAPE));

  ExtensionTestMessageListener second_key_listener("B_KEY_RECEIVED", false);

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_B));

  ASSERT_TRUE(second_key_listener.WaitUntilSatisfied());

  // We assume that at that point, if we had to leave fullscreen, we should be.
  // However, by nature, we can not guarantee that and given that we do test
  // that nothing happens, we might end up with random-success when the feature
  // is broken.
  EXPECT_TRUE(GetFirstShellWindow()->GetBaseWindow()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest,
                       MAYBE_ESCDoesNotLeaveFullscreenDOM) {
  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("prevent_leave_fullscreen");
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  launched_listener.Reply("dom");

  // Because the DOM way to go fullscreen requires user gesture, we simulate a
  // key event to get the window entering in fullscreen mode. The reply will
  // make the window listen for the key event. The reply will be sent to the
  // renderer process before the keypress and should be received in that order.
  // When receiving the key event, the application will try to go fullscreen
  // using the Window API but there is no synchronous way to know if that
  // actually succeeded. Also, failure will not be notified. A failure case will
  // only be known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstShellWindow()->GetBaseWindow());

    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_A));

    fs_changed.Wait();
  }

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_ESCAPE));

  ExtensionTestMessageListener second_key_listener("B_KEY_RECEIVED", false);

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_B));

  ASSERT_TRUE(second_key_listener.WaitUntilSatisfied());

  // We assume that at that point, if we had to leave fullscreen, we should be.
  // However, by nature, we can not guarantee that and given that we do test
  // that nothing happens, we might end up with random-success when the feature
  // is broken.
  EXPECT_TRUE(GetFirstShellWindow()->GetBaseWindow()->IsFullscreen());
}

// This test does not work on Linux Aura because ShowInactive() is not
// implemented. See http://crbug.com/325142
// It also does not work on Windows because of the document being focused even
// though the window is not activated. See http://crbug.com/326986
// It also does not work on MacOS because ::ShowInactive() ends up behaving like
// ::Show() because of Cocoa conventions. See http://crbug.com/326987
#if (defined(OS_LINUX) && defined(USE_AURA)) || \
    defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_TestCreate DISABLED_TestCreate
#else
#define MAYBE_TestCreate TestCreate
#endif

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest, MAYBE_TestCreate) {
  ASSERT_TRUE(RunAppWindowInteractiveTest("testCreate")) << message_;
}
