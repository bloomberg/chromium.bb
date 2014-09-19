// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "extensions/test/extension_test_message_listener.h"

// This class of BrowserTests is a helper to create tests related to crashes in
// Chrome Apps. To be tested, the app will have to be placed as any other test
// app (see PlatformAppBrowserTest) and will need to send a "Done" message back.
// When the "Done" message is received, the test succeed. If it is not, it is
// assumed that Chrome has crashed and the test will anyway fail.
//
// The entry in this file should be something like:
// IN_PROC_BROWSER_TEST_F(AppCrashTest, <TEST_NAME>) {
//   ASSERT_TRUE(RunAppCrashTest("<DIRECTORY_TEST_NAME>"));
// }

class AppCrashTest : public extensions::PlatformAppBrowserTest {
public:
 void RunAppCrashTest(const char* name) {
   LoadAndLaunchPlatformApp(name, "Done");
  }
};

IN_PROC_BROWSER_TEST_F(AppCrashTest, HiddenWindows) {
  RunAppCrashTest("crashtest_hidden_windows");
}
