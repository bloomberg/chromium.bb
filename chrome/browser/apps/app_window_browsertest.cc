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

IN_PROC_BROWSER_TEST_F(AppWindowAPI, TestCreate) {
  ASSERT_TRUE(RunAppWindowAPITest("testCreate")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, TestSingleton) {
  ASSERT_TRUE(RunAppWindowAPITest("testSingleton")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, TestBounds) {
  ASSERT_TRUE(RunAppWindowAPITest("testBounds")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, TestCloseEvent) {
  ASSERT_TRUE(RunAppWindowAPITest("testCloseEvent")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, TestMaximize) {
  ASSERT_TRUE(RunAppWindowAPITest("testMaximize")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, TestRestore) {
  ASSERT_TRUE(RunAppWindowAPITest("testRestore")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, TestRestoreAfterClose) {
  ASSERT_TRUE(RunAppWindowAPITest("testRestoreAfterClose")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPI, TestSizeConstraints) {
  ASSERT_TRUE(RunAppWindowAPITest("testSizeConstraints")) << message_;
}
