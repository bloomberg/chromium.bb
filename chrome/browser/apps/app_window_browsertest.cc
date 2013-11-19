// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"

// Helper class for tests related to the Apps Window API (chrome.app.window).
class AppWindowAPI : public extensions::PlatformAppBrowserTest {
 public:
  bool RunAppWindowAPITest(const char* testName) {
    ExtensionTestMessageListener launched_listener("Launched", true);
    LoadAndLaunchPlatformApp("window_api");
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
};

// These tests are flaky after https://codereview.chromium.org/57433010/.
// See http://crbug.com/319613.

IN_PROC_BROWSER_TEST_F(AppWindowAPI, DISABLED_TestCreate) {
  ASSERT_TRUE(RunAppWindowAPITest("testCreate")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, DISABLED_TestSingleton) {
  ASSERT_TRUE(RunAppWindowAPITest("testSingleton")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, DISABLED_TestBounds) {
  ASSERT_TRUE(RunAppWindowAPITest("testBounds")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, DISABLED_TestCloseEvent) {
  ASSERT_TRUE(RunAppWindowAPITest("testCloseEvent")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, DISABLED_TestMaximize) {
  ASSERT_TRUE(RunAppWindowAPITest("testMaximize")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, DISABLED_TestRestore) {
  ASSERT_TRUE(RunAppWindowAPITest("testRestore")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, DISABLED_TestRestoreAfterClose) {
  ASSERT_TRUE(RunAppWindowAPITest("testRestoreAfterClose")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, DISABLED_TestSizeConstraints) {
  ASSERT_TRUE(RunAppWindowAPITest("testSizeConstraints")) << message_;
}
