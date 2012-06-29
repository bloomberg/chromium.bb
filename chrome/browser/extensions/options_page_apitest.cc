// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"

using extensions::Extension;

// Used to simulate a click on the first button named 'Options'.
static const wchar_t* jscript_click_option_button =
    L"(function() { "
    L"  var button = document.evaluate(\"//button[text()='Options']\","
    L"      document, null, XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE,"
    L"      null).snapshotItem(0);"
    L"  button.click();"
    L"})();";

// Test that an extension with an options page makes an 'Options' button appear
// on chrome://extensions, and that clicking the button opens a new tab with the
// extension's options page.
// Disabled.  See http://crbug.com/26948 for details.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, DISABLED_OptionsPage) {
  // Install an extension with an options page.
  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("options.crx"), 1);
  ASSERT_TRUE(extension);
  ExtensionService* service = browser()->profile()->GetExtensionService();
  ASSERT_EQ(1u, service->extensions()->size());

  // Go to the Extension Settings page and click the Options button.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIExtensionsURL));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      chrome::GetActiveWebContents(browser())->GetRenderViewHost(), L"",
      jscript_click_option_button));

  // If the options page hasn't already come up, wait for it.
  if (tab_strip->count() == 1) {
    ui_test_utils::WaitForNewTab(browser());
  }
  ASSERT_EQ(2, tab_strip->count());

  EXPECT_EQ(extension->GetResourceURL("options.html"),
            tab_strip->GetTabContentsAt(1)->web_contents()->GetURL());
}
