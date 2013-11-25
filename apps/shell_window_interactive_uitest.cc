// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/ui/native_app_window.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"

using extensions::PlatformAppBrowserTest;
using extensions::Extension;

namespace apps {

namespace {

const char kDefaultTestApp[] = "shell_window/generic";

class ShellWindowTest : public PlatformAppBrowserTest {
 protected:
  // Load the test app and create a window. The window must be closed by the
  // caller in order to terminate the test.
  // |app_path| is the name of the test.
  // |window_create_options| are the options that will be passed to
  // chrome.app.window.create() in the test app.
  ShellWindow* OpenWindow(const char* app_path,
                          const char* window_create_options) {
    ExtensionTestMessageListener launched_listener("launched", true);
    ExtensionTestMessageListener loaded_listener("window_loaded", false);

    // Load and launch the test app.
    const Extension* extension = LoadAndLaunchPlatformApp(app_path);
    EXPECT_TRUE(extension);
    EXPECT_TRUE(launched_listener.WaitUntilSatisfied());

    // Send the options for window creation.
    launched_listener.Reply(window_create_options);

    // Wait for the window to be opened and loaded.
    EXPECT_TRUE(loaded_listener.WaitUntilSatisfied());

    EXPECT_EQ(1U, GetShellWindowCount());
    ShellWindow* shell_window = GetFirstShellWindow();
    return shell_window;
  }

  void CheckAlwaysOnTopToFullscreen(ShellWindow* window) {
    ASSERT_TRUE(window->GetBaseWindow()->IsAlwaysOnTop());

    // The always-on-top property should be temporarily disabled when the window
    // enters fullscreen.
    window->Fullscreen();
    EXPECT_FALSE(window->GetBaseWindow()->IsAlwaysOnTop());

    // From the API's point of view, the always-on-top property is enabled.
    EXPECT_TRUE(window->IsAlwaysOnTop());

    // The always-on-top property is restored when the window exits fullscreen.
    window->Restore();
    EXPECT_TRUE(window->GetBaseWindow()->IsAlwaysOnTop());
  }

  void CheckNormalToFullscreen(ShellWindow* window) {
    // If the always-on-top property is false, it should remain this way when
    // entering and exiting fullscreen mode.
    ASSERT_FALSE(window->GetBaseWindow()->IsAlwaysOnTop());
    window->Fullscreen();
    EXPECT_FALSE(window->GetBaseWindow()->IsAlwaysOnTop());
    window->Restore();
    EXPECT_FALSE(window->GetBaseWindow()->IsAlwaysOnTop());
  }

  void CheckFullscreenToAlwaysOnTop(ShellWindow* window) {
    ASSERT_TRUE(window->GetBaseWindow()->IsFullscreenOrPending());

    // Now enable always-on-top at runtime and ensure the property does not get
    // applied immediately because the window is in fullscreen mode.
    window->SetAlwaysOnTop(true);
    EXPECT_FALSE(window->GetBaseWindow()->IsAlwaysOnTop());

    // From the API's point of view, the always-on-top property is enabled.
    EXPECT_TRUE(window->IsAlwaysOnTop());

    // Ensure the always-on-top property is applied when exiting fullscreen.
    window->Restore();
    EXPECT_TRUE(window->GetBaseWindow()->IsAlwaysOnTop());
  }
};

}  // namespace

// Tests are flaky on Mac as transitioning to fullscreen is not instantaneous
// and throws errors when entering/exiting fullscreen too quickly.
#if defined(OS_MACOSX)
#define MAYBE_InitAlwaysOnTopToFullscreen DISABLED_InitAlwaysOnTopToFullscreen
#else
#define MAYBE_InitAlwaysOnTopToFullscreen InitAlwaysOnTopToFullscreen
#endif

// Tests a window created with always-on-top enabled and ensures that the
// property is temporarily switched off when entering fullscreen mode.
IN_PROC_BROWSER_TEST_F(ShellWindowTest, MAYBE_InitAlwaysOnTopToFullscreen) {
  ShellWindow* window = OpenWindow(
      kDefaultTestApp, "{ \"alwaysOnTop\": true }");
  ASSERT_TRUE(window);
  CheckAlwaysOnTopToFullscreen(window);

  window->SetAlwaysOnTop(false);
  CheckNormalToFullscreen(window);

  CloseShellWindow(window);
}

#if defined(OS_MACOSX)
#define MAYBE_RuntimeAlwaysOnTopToFullscreen DISABLED_RuntimeAlwaysOnTopToFullscreen
#else
#define MAYBE_RuntimeAlwaysOnTopToFullscreen RuntimeAlwaysOnTopToFullscreen
#endif

// Tests a window with always-on-top enabled at runtime and ensures that the
// property is temporarily switched off when entering fullscreen mode.
IN_PROC_BROWSER_TEST_F(ShellWindowTest, MAYBE_RuntimeAlwaysOnTopToFullscreen) {
  ShellWindow* window = OpenWindow(
      kDefaultTestApp, "{}");
  ASSERT_TRUE(window);
  CheckNormalToFullscreen(window);

  window->SetAlwaysOnTop(true);
  CheckAlwaysOnTopToFullscreen(window);

  CloseShellWindow(window);
}

#if defined(OS_MACOSX)
#define MAYBE_InitFullscreenToAlwaysOnTop DISABLED_InitFullscreenToAlwaysOnTop
#else
#define MAYBE_InitFullscreenToAlwaysOnTop InitFullscreenToAlwaysOnTop
#endif

// Tests a window created initially in fullscreen mode and ensures that the
// always-on-top property does not get applied until it exits fullscreen.
IN_PROC_BROWSER_TEST_F(ShellWindowTest, MAYBE_InitFullscreenToAlwaysOnTop) {
  ShellWindow* window = OpenWindow(
      kDefaultTestApp, "{ \"state\": \"fullscreen\" }");
  ASSERT_TRUE(window);
  CheckFullscreenToAlwaysOnTop(window);

  CloseShellWindow(window);
}

#if defined(OS_MACOSX)
#define MAYBE_RuntimeFullscreenToAlwaysOnTop DISABLED_RuntimeFullscreenToAlwaysOnTop
#else
#define MAYBE_RuntimeFullscreenToAlwaysOnTop RuntimeFullscreenToAlwaysOnTop
#endif

// Tests a window that enters fullscreen mode at runtime and ensures that the
// always-on-top property does not get applied until it exits fullscreen.
IN_PROC_BROWSER_TEST_F(ShellWindowTest, MAYBE_RuntimeFullscreenToAlwaysOnTop) {
  ShellWindow* window = OpenWindow(
      kDefaultTestApp, "{}");
  ASSERT_TRUE(window);

  window->Fullscreen();
  CheckFullscreenToAlwaysOnTop(window);

  CloseShellWindow(window);
}

#if defined(OS_MACOSX)
#define MAYBE_InitFullscreenAndAlwaysOnTop DISABLED_InitFullscreenAndAlwaysOnTop
#else
#define MAYBE_InitFullscreenAndAlwaysOnTop InitFullscreenAndAlwaysOnTop
#endif

// Tests a window created with both fullscreen and always-on-top enabled. Ensure
// that always-on-top is only applied when the window exits fullscreen.
IN_PROC_BROWSER_TEST_F(ShellWindowTest, MAYBE_InitFullscreenAndAlwaysOnTop) {
  ShellWindow* window = OpenWindow(
      kDefaultTestApp, "{ \"alwaysOnTop\": true, \"state\": \"fullscreen\" }");
  ASSERT_TRUE(window);

  EXPECT_TRUE(window->GetBaseWindow()->IsFullscreenOrPending());
  EXPECT_FALSE(window->GetBaseWindow()->IsAlwaysOnTop());

  // From the API's point of view, the always-on-top property is enabled.
  EXPECT_TRUE(window->IsAlwaysOnTop());

  window->Restore();
  EXPECT_TRUE(window->GetBaseWindow()->IsAlwaysOnTop());

  CloseShellWindow(window);
}

#if defined(OS_MACOSX)
#define MAYBE_DisableAlwaysOnTopInFullscreen DISABLED_DisableAlwaysOnTopInFullscreen
#else
#define MAYBE_DisableAlwaysOnTopInFullscreen DisableAlwaysOnTopInFullscreen
#endif

// Tests a window created with always-on-top enabled, but then disabled while
// in fullscreen mode. After exiting fullscreen, always-on-top should remain
// disabled.
IN_PROC_BROWSER_TEST_F(ShellWindowTest, MAYBE_DisableAlwaysOnTopInFullscreen) {
  ShellWindow* window = OpenWindow(
      kDefaultTestApp, "{ \"alwaysOnTop\": true }");
  ASSERT_TRUE(window);

  // Disable always-on-top while in fullscreen mode.
  window->Fullscreen();
  EXPECT_FALSE(window->GetBaseWindow()->IsAlwaysOnTop());
  window->SetAlwaysOnTop(false);
  EXPECT_FALSE(window->GetBaseWindow()->IsAlwaysOnTop());

  // Ensure that always-on-top remains disabled.
  window->Restore();
  EXPECT_FALSE(window->GetBaseWindow()->IsAlwaysOnTop());

  CloseShellWindow(window);
}

}  // namespace apps
