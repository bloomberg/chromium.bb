// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

namespace extensions {

class PushMessagingApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(PushMessagingApiTest, EventDispatch) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ExtensionTestMessageListener ready("ready", true);
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging"));
  ASSERT_TRUE(extension);
  GURL page_url = extension->GetResourceURL("event_dispatch.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  EXPECT_TRUE(ready.WaitUntilSatisfied());

  // Trigger a callback.
  browser()->profile()->GetExtensionService()->
      push_messaging_event_router()->OnMessage(
          extension->id(), 1, "payload");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
