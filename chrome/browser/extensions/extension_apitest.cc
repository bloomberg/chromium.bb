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
  // Note the inner scope here. The |registrar| will fall out of scope and
  // remove listeners *before* the call to WaitForPassFail() below.
  {
    LOG(INFO) << "Running ExtensionApiTest with: " << extension_name;
    NotificationRegistrar registrar;
    registrar.Add(this, NotificationType::EXTENSION_TEST_PASSED,
                  NotificationService::AllSources());
    registrar.Add(this, NotificationType::EXTENSION_TEST_FAILED,
                  NotificationService::AllSources());

    if (!LoadExtension(test_data_dir_.AppendASCII(extension_name))) {
      message_ = "Failed to load extension.";
      return false;
    }
  }

  // TODO(erikkay) perhaps we shouldn't do this implicitly.
  return WaitForPassFail();
}

bool ExtensionApiTest::WaitForPassFail() {
  NotificationRegistrar registrar;
  registrar.Add(this, NotificationType::EXTENSION_TEST_PASSED,
                NotificationService::AllSources());
  registrar.Add(this, NotificationType::EXTENSION_TEST_FAILED,
                NotificationService::AllSources());

  // Depending on the tests, multiple results can come in from a single call
  // to RunMessageLoop(), so we maintain a queue of results and just pull them
  // off as the test calls this, going to the run loop only when the queue is
  // empty.
  if (!results_.size()) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, new MessageLoop::QuitTask, kTimeoutMs);
    ui_test_utils::RunMessageLoop();
  }
  if (results_.size()) {
    bool ret = results_.front();
    results_.pop_front();
    message_ = messages_.front();
    messages_.pop_front();
    return ret;
  }
  message_ = "No response from message loop.";
  return false;
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
      results_.push_back(true);
      messages_.push_back("");
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_TEST_FAILED:
      std::cout << "Got EXTENSION_TEST_FAILED notification.\n";
      results_.push_back(false);
      messages_.push_back(*(Details<std::string>(details).ptr()));
      MessageLoopForUI::current()->Quit();
      break;

    default:
      ExtensionBrowserTest::Observe(type, source, details);
  }
}
