// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

// Times out on win syzyasan, http://crbug.com/166026
#if defined(SYZYASAN)
#define MAYBE_ActiveTab DISABLED_ActiveTab
#else
#define MAYBE_ActiveTab ActiveTab
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_ActiveTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("active_tab"));
  ASSERT_TRUE(extension);

  // Shouldn't be initially granted based on activeTab.
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "/extensions/api_test/active_tab/page.html"));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  // Granting to the extension should give it access to page.html.
  {
    ResultCatcher catcher;
    ExtensionActionAPI::Get(browser()->profile())->ExecuteExtensionAction(
        extension, browser(), true);
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  // Changing page should go back to it not having access.
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "/extensions/api_test/active_tab/final_page.html"));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }
}

}  // namespace
}  // namespace extensions
