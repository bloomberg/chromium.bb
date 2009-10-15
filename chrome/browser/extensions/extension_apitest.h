// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_APITEST_H_

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/common/notification_service.h"

// The general flow of these API tests should work like this:
// (1) Setup initial browser state (e.g. create some bookmarks for the
//     bookmark test)
// (2) Call ASSERT_TRUE(RunExtensionTest(name));
// (3) In your extension code, run your test and call chrome.test.pass or
//     chrome.test.fail
// (4) Verify expected browser state.
// TODO(erikkay): There should also be a way to drive events in these tests.

class ExtensionApiTest : public ExtensionBrowserTest {
 protected:
  // Load |extension_name| and wait for pass / fail notification.
  // |extension_name| is a directory in "test/data/extensions/api_test".
  bool RunExtensionTest(const char* extension_name);

  // Reset |completed_| and wait for a new pass / fail notification.
  bool WaitForPassFail();

  // All extensions tested by ExtensionApiTest are in the "api_test" dir.
  virtual void SetUpCommandLine(CommandLine* command_line);

  // NotificationObserver
  void Observe(NotificationType type, const NotificationSource& source,
               const NotificationDetails& details);

  // A sequential list of pass/fail notifications from the test extension(s).
  std::deque<bool> results_;

  // If it failed, what was the error message?
  std::deque<std::string> messages_;
  std::string message_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_APITEST_H_
