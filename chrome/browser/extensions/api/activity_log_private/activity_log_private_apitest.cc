// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_builder.h"

namespace extensions {

class ActivityLogApiTest : public ExtensionApiTest {
 public:
  ActivityLogApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogTesting);
  }
};

// The test extension sends a message to its 'friend'. The test completes
// if it successfully sees the 'friend' receive the message.
IN_PROC_BROWSER_TEST_F(ActivityLogApiTest, TriggerEvent) {
  const Extension* friend_extension =
      LoadExtension(test_data_dir_.AppendASCII("activity_log_private/friend"));
  ASSERT_TRUE(friend_extension);
  ASSERT_TRUE(RunExtensionTest("activity_log_private/test"));
}

}  // namespace extensions

