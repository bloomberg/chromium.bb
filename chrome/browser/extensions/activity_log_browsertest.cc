// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

// Used to fire all of the listeners on the buttons.
static const char kScriptClickAllTestButtons[] =
    "(function() {"
    "  setRunningAsRobot();"
    "  var buttons = document.getElementsByTagName('button');"
    "  for (var i=0; i < buttons.length; i++) {"
    "    buttons[i].click();"
    "  }"
    "})();";

class ActivityLogExtensionTest : public ExtensionApiTest {
 protected:
  // Make sure the activity log is turned on.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
  }
};

namespace extensions {

IN_PROC_BROWSER_TEST_F(ActivityLogExtensionTest, ExtensionEndToEnd) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartTestServer();

  // Get the extension (chrome/test/data/extensions/activity_log)
  const Extension* ext =
      LoadExtension(test_data_dir_.AppendASCII("activity_log"));
  ASSERT_TRUE(ext);

  // Open up the Options page.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://" + ext->id() + "/options.html"));
  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Run the test by firing all the buttons.  Wait until completion.
  ResultCatcher catcher;
  ASSERT_TRUE(content::ExecuteScript(tab_strip->GetActiveWebContents(),
                                     kScriptClickAllTestButtons));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
