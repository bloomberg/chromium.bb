// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

class DefaultPluginUITest : public UITest {
 public:
  DefaultPluginUITest() {
    dom_automation_enabled_ = true;
  }
};

#if defined(OS_WIN)
#define MAYBE_DefaultPluginLoadTest DISABLED_DefaultPluginLoadTest
#else
#define MAYBE_DefaultPluginLoadTest DefaultPluginLoadTest
#endif
TEST_F(DefaultPluginUITest, MAYBE_DefaultPluginLoadTest) {
  // Open page with default plugin.
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("default_plugin.html");
  NavigateToURL(net::FilePathToFileURL(test_file));

  // Check that the default plugin was loaded. It executes a bit of javascript
  // in the HTML file which replaces the innerHTML of div |result| with "DONE".
  scoped_refptr<TabProxy> tab(GetActiveTab());
  string16 out;
  ASSERT_TRUE(tab->ExecuteAndExtractString(string16(),
      ASCIIToUTF16("domAutomationController.send("
                   "document.getElementById('result').innerHTML)"), &out));
  ASSERT_EQ(ASCIIToUTF16("DONE"), out);
}
