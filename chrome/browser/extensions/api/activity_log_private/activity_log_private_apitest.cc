// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_builder.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class ActivityLogApiTest : public ExtensionApiTest {
 public:
  ActivityLogApiTest() : saved_cmdline_(CommandLine::NO_PROGRAM) {}

  virtual ~ActivityLogApiTest() {
    ExtensionApiTest::SetUpCommandLine(&saved_cmdline_);
    *CommandLine::ForCurrentProcess() = saved_cmdline_;
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    saved_cmdline_ = *CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
  }

 private:
  CommandLine saved_cmdline_;
};

// The test extension sends a message to its 'friend'. The test completes
// if it successfully sees the 'friend' receive the message.
IN_PROC_BROWSER_TEST_F(ActivityLogApiTest, TriggerEvent) {
  ActivityLog::GetInstance(profile())->SetWatchdogAppActive(true);
  host_resolver()->AddRule("*", "127.0.0.1");
  const Extension* friend_extension = LoadExtensionIncognito(
      test_data_dir_.AppendASCII("activity_log_private/friend"));
  ASSERT_TRUE(friend_extension);
  ASSERT_TRUE(RunExtensionTest("activity_log_private/test"));
  ActivityLog::GetInstance(profile())->SetWatchdogAppActive(false);
}

}  // namespace extensions

