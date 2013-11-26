// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/ui/native_app_window.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/test/base/interactive_test_utils.h"

using namespace apps;

// This test does not work on Linux Aura yet. It might be because the fullscreen
// window is not correctly focused or because key events are not correctly sent.
#if !(defined(OS_LINUX) && defined(USE_AURA))

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

class AppInteractiveTest : public extensions::PlatformAppBrowserTest {
 public:
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

IN_PROC_BROWSER_TEST_F(AppInteractiveTest, ESCLeavesFullscreenWindow) {
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

IN_PROC_BROWSER_TEST_F(AppInteractiveTest, ESCLeavesFullscreenDOM) {
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

IN_PROC_BROWSER_TEST_F(AppInteractiveTest, ESCDoesNotLeaveFullscreenWindow) {
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

IN_PROC_BROWSER_TEST_F(AppInteractiveTest, ESCDoesNotLeaveFullscreenDOM) {
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

#endif // !(defined(OS_LINUX) && defined(USE_AURA))
