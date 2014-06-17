// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/ui/native_app_window.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/test/base/interactive_test_utils.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

using apps::NativeAppWindow;

// Helper class that has to be created in the stack to check if the fullscreen
// setting of a NativeWindow has changed since the creation of the object.
class FullscreenChangeWaiter {
 public:
  explicit FullscreenChangeWaiter(NativeAppWindow* window)
      : window_(window),
        initial_fullscreen_state_(window_->IsFullscreen()) {}

  void Wait() {
    while (initial_fullscreen_state_ == window_->IsFullscreen())
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
    LoadAndLaunchPlatformApp("window_api_interactive", &launched_listener);

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
        GetFirstAppWindow()->GetNativeWindow(),
        key,
        false,
        false,
        false,
        false);
  }

  // This method will wait until the application is able to ack a key event.
  void WaitUntilKeyFocus() {
    ExtensionTestMessageListener key_listener("KeyReceived", false);

    while (!key_listener.was_satisfied()) {
      ASSERT_TRUE(SimulateKeyPress(ui::VKEY_Z));
      content::RunAllPendingInMessageLoop();
    }
  }
};

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest, ESCLeavesFullscreenWindow) {
// This test is flaky on MacOS 10.6.
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (base::mac::IsOSSnowLeopard())
    return;
#endif

  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("leave_fullscreen", &launched_listener);

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow()->GetNativeWindow()));

  // When receiving the reply, the application will try to go fullscreen using
  // the Window API but there is no synchronous way to know if that actually
  // succeeded. Also, failure will not be notified. A failure case will only be
  // known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    launched_listener.Reply("window");

    fs_changed.Wait();
  }

  // Depending on the platform, going fullscreen might create an animation.
  // We want to make sure that the ESC key we will send next is actually going
  // to be received and the application might not receive key events during the
  // animation so we should wait for the key focus to be back.
  WaitUntilKeyFocus();

  // Same idea as above but for leaving fullscreen. Fullscreen mode should be
  // left when ESC is received.
  {
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_ESCAPE));

    fs_changed.Wait();
  }
}

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest, ESCLeavesFullscreenDOM) {
// This test is flaky on MacOS 10.6.
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (base::mac::IsOSSnowLeopard())
    return;
#endif

  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("leave_fullscreen", &launched_listener);

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow()->GetNativeWindow()));

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
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    WaitUntilKeyFocus();
    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_A));

    fs_changed.Wait();
  }

  // Depending on the platform, going fullscreen might create an animation.
  // We want to make sure that the ESC key we will send next is actually going
  // to be received and the application might not receive key events during the
  // animation so we should wait for the key focus to be back.
  WaitUntilKeyFocus();

  // Same idea as above but for leaving fullscreen. Fullscreen mode should be
  // left when ESC is received.
  {
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_ESCAPE));

    fs_changed.Wait();
  }
}

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest,
                       ESCDoesNotLeaveFullscreenWindow) {
// This test is flaky on MacOS 10.6.
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (base::mac::IsOSSnowLeopard())
    return;
#endif

  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("prevent_leave_fullscreen", &launched_listener);

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow()->GetNativeWindow()));

  // When receiving the reply, the application will try to go fullscreen using
  // the Window API but there is no synchronous way to know if that actually
  // succeeded. Also, failure will not be notified. A failure case will only be
  // known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    launched_listener.Reply("window");

    fs_changed.Wait();
  }

  // Depending on the platform, going fullscreen might create an animation.
  // We want to make sure that the ESC key we will send next is actually going
  // to be received and the application might not receive key events during the
  // animation so we should wait for the key focus to be back.
  WaitUntilKeyFocus();

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_ESCAPE));

  ExtensionTestMessageListener second_key_listener("B_KEY_RECEIVED", false);

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_B));

  ASSERT_TRUE(second_key_listener.WaitUntilSatisfied());

  // We assume that at that point, if we had to leave fullscreen, we should be.
  // However, by nature, we can not guarantee that and given that we do test
  // that nothing happens, we might end up with random-success when the feature
  // is broken.
  EXPECT_TRUE(GetFirstAppWindow()->GetBaseWindow()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest,
                       ESCDoesNotLeaveFullscreenDOM) {
// This test is flaky on MacOS 10.6.
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (base::mac::IsOSSnowLeopard())
    return;
#endif

  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("prevent_leave_fullscreen", &launched_listener);

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow()->GetNativeWindow()));

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
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    WaitUntilKeyFocus();
    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_A));

    fs_changed.Wait();
  }

  // Depending on the platform, going fullscreen might create an animation.
  // We want to make sure that the ESC key we will send next is actually going
  // to be received and the application might not receive key events during the
  // animation so we should wait for the key focus to be back.
  WaitUntilKeyFocus();

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_ESCAPE));

  ExtensionTestMessageListener second_key_listener("B_KEY_RECEIVED", false);

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_B));

  ASSERT_TRUE(second_key_listener.WaitUntilSatisfied());

  // We assume that at that point, if we had to leave fullscreen, we should be.
  // However, by nature, we can not guarantee that and given that we do test
  // that nothing happens, we might end up with random-success when the feature
  // is broken.
  EXPECT_TRUE(GetFirstAppWindow()->GetBaseWindow()->IsFullscreen());
}

// This test is duplicated from ESCDoesNotLeaveFullscreenWindow.
// It runs the same test, but uses the old permission names: 'fullscreen'
// and 'overrideEscFullscreen'.
IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest,
                       ESCDoesNotLeaveFullscreenOldPermission) {
// This test is flaky on MacOS 10.6.
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (base::mac::IsOSSnowLeopard())
    return;
#endif

  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("prevent_leave_fullscreen_old", &launched_listener);

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow()->GetNativeWindow()));

  // When receiving the reply, the application will try to go fullscreen using
  // the Window API but there is no synchronous way to know if that actually
  // succeeded. Also, failure will not be notified. A failure case will only be
  // known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    launched_listener.Reply("window");

    fs_changed.Wait();
  }

  // Depending on the platform, going fullscreen might create an animation.
  // We want to make sure that the ESC key we will send next is actually going
  // to be received and the application might not receive key events during the
  // animation so we should wait for the key focus to be back.
  WaitUntilKeyFocus();

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_ESCAPE));

  ExtensionTestMessageListener second_key_listener("B_KEY_RECEIVED", false);

  ASSERT_TRUE(SimulateKeyPress(ui::VKEY_B));

  ASSERT_TRUE(second_key_listener.WaitUntilSatisfied());

  // We assume that at that point, if we had to leave fullscreen, we should be.
  // However, by nature, we can not guarantee that and given that we do test
  // that nothing happens, we might end up with random-success when the feature
  // is broken.
  EXPECT_TRUE(GetFirstAppWindow()->GetBaseWindow()->IsFullscreen());
}

// This test does not work on Linux Aura because ShowInactive() is not
// implemented. See http://crbug.com/325142
// It also does not work on Windows because of the document being focused even
// though the window is not activated. See http://crbug.com/326986
// It also does not work on MacOS because ::ShowInactive() ends up behaving like
// ::Show() because of Cocoa conventions. See http://crbug.com/326987
// Those tests should be disabled on Linux GTK when they are enabled on the
// other platforms, see http://crbug.com/328829
#if (defined(OS_LINUX) && defined(USE_AURA)) || \
    defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_TestCreate DISABLED_TestCreate
#define MAYBE_TestShow DISABLED_TestShow
#else
#define MAYBE_TestCreate TestCreate
#define MAYBE_TestShow TestShow
#endif

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest, MAYBE_TestCreate) {
  ASSERT_TRUE(RunAppWindowInteractiveTest("testCreate")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowInteractiveTest, MAYBE_TestShow) {
  ASSERT_TRUE(RunAppWindowInteractiveTest("testShow")) << message_;
}
