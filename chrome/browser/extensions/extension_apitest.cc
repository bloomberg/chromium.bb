// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/browser.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/test/ui_test_utils.h"

namespace {
static const int kTimeoutMs = 60 * 1000;  // 1 minute
};

// Load an extension and wait for it to notify of PASSED or FAILED.
bool ExtensionApiTest::RunExtensionTest(const char* extension_name) {
  bool result;
  completed_ = false;
  {
    NotificationRegistrar registrar;
    registrar.Add(this, NotificationType::EXTENSION_TEST_PASSED,
                  NotificationService::AllSources());
    registrar.Add(this, NotificationType::EXTENSION_TEST_FAILED,
                  NotificationService::AllSources());
    result = LoadExtension(test_data_dir_.AppendASCII(extension_name));

    // If the test runs quickly, we may get the notification while waiting
    // for the Load to finish.
    if (completed_) {
      result = passed_;
    } else {
      result = WaitForPassFail();
    }
  }
  return result;
}

bool ExtensionApiTest::WaitForPassFail() {
  completed_ = false;
  passed_ = false;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, new MessageLoop::QuitTask, kTimeoutMs);
  ui_test_utils::RunMessageLoop();
  return passed_;
}

void ExtensionApiTest::SetUpCommandLine(CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  test_data_dir_ = test_data_dir_.AppendASCII("api_test");
}

void ExtensionApiTest::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_TEST_PASSED:
      std::cout << "Got EXTENSION_TEST_PASSED notification.\n";
      completed_ = true;
      passed_ = true;
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_TEST_FAILED:
      std::cout << "Got EXTENSION_TEST_FAILED notification.\n";
      completed_ = true;
      passed_ = false;
      message_ = *(Details<std::string>(details).ptr());
      MessageLoopForUI::current()->Quit();
      break;

    default:
      ExtensionBrowserTest::Observe(type, source, details);
  }
}
