// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"

// Tests that we can load extension pages into the tab area and they can call
// extension APIs.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WebContents) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      chrome::GetActiveWebContents(browser()),
      "testTabsAPI()",
      &result));
  EXPECT_TRUE(result);

  // There was a bug where we would crash if we navigated to a page in the same
  // extension because no new render view was getting created, so we would not
  // do some setup.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));
  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      chrome::GetActiveWebContents(browser()),
      "testTabsAPI()",
      &result));
  EXPECT_TRUE(result);
}
