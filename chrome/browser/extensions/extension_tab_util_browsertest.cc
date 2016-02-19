// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/manifest_handlers/options_page_info.h"

namespace extensions {

using ExtensionTabUtilBrowserTest = ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(ExtensionTabUtilBrowserTest, OpenExtensionsOptionsPage) {
  // Load an extension with an options page.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("options_page"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(OptionsPageInfo::HasOptionsPage(extension));

  // Start at the new tab page, and then open the extension options page.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  GURL options_url = OptionsPageInfo::GetOptionsPage(extension);
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(extension, browser()));

  // Opening the options page should take the new tab and use it, so we should
  // have only one tab, and it should be open to the options page.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
                  browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url,
            browser()->tab_strip_model()->
                GetActiveWebContents()->GetLastCommittedURL());

  // Calling OpenOptionsPage again shouldn't result in any new tabs, since we
  // re-use the existing options page.
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(extension, browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
                  browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url,
            browser()->tab_strip_model()->
                GetActiveWebContents()->GetLastCommittedURL());

  // Navigate to google.com (something non-newtab, non-options). Calling
  // OpenOptionsPage() should create a new tab and navigate it to the options
  // page. So we should have two total tabs, with the active tab pointing to
  // options.
  ui_test_utils::NavigateToURL(browser(), GURL("http://www.google.com/"));
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(extension, browser()));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
                  browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url,
            browser()->tab_strip_model()->
                GetActiveWebContents()->GetLastCommittedURL());

  // Navigate the tab to a different extension URL, and call OpenOptionsPage().
  // We should not reuse the current tab since it's opened to a page that isn't
  // the options page, and we don't want to arbitrarily close extension content.
  // Regression test for crbug.com/587581.
  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("other.html"));
  EXPECT_TRUE(ExtensionTabUtil::OpenOptionsPage(extension, browser()));
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
                  browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(options_url,
            browser()->tab_strip_model()->
                GetActiveWebContents()->GetLastCommittedURL());

  // If the user navigates to the options page e.g. by typing in the url, it
  // should not override the currently-open tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      options_url,
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(4, browser()->tab_strip_model()->count());
  EXPECT_EQ(options_url,
            browser()->tab_strip_model()->
                GetActiveWebContents()->GetLastCommittedURL());
}

}  // namespace extensions
