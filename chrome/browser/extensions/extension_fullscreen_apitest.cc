// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ExtensionFullscreenAccessFail) {
  // Test that fullscreen can be accessed from an extension without permission.
  ASSERT_TRUE(RunPlatformAppTest("fullscreen/no_permission")) << message_;
}

// Disabled, a user gesture is required for fullscreen. http://crbug.com/174178
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       DISABLED_ExtensionFullscreenAccessPass) {
  // Test that fullscreen can be accessed from an extension with permission.
  ASSERT_TRUE(RunPlatformAppTest("fullscreen/has_permission")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       FocusWindowDoesNotExitFullscreen) {
  browser()->window()->EnterFullscreen(
      GURL(), FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION);
  bool is_fullscreen = browser()->window()->IsFullscreen();
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_EQ(is_fullscreen, browser()->window()->IsFullscreen());
}

// Fails flakily: http://crbug.com/308041
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       DISABLED_UpdateWindowSizeExitsFullscreen) {
  browser()->window()->EnterFullscreen(
      GURL(), FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION);
  ASSERT_TRUE(RunExtensionTest("window_update/sizing")) << message_;
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}
