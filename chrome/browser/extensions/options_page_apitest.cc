// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"

using extensions::Extension;

// Used to simulate a click on the first button named 'Options'.
static const char kScriptClickOptionButton[] =
    "(function() { "
    "  var button = document.evaluate(\"//button[text()='Options']\","
    "      document, null, XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE,"
    "      null).snapshotItem(0);"
    "  button.click();"
    "})();";

// Test that an extension with an options page makes an 'Options' button appear
// on chrome://extensions, and that clicking the button opens a new tab with the
// extension's options page.
// Disabled.  See http://crbug.com/26948 for details.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, DISABLED_OptionsPage) {
  // Install an extension with an options page.
  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("options.crx"), 1);
  ASSERT_TRUE(extension);
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  ASSERT_EQ(1u, service->extensions()->size());

  // Go to the Extension Settings page and click the Options button.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIExtensionsURL));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_TRUE(content::ExecuteJavaScript(
      tab_strip->GetActiveWebContents()->GetRenderViewHost(),
      "",
      kScriptClickOptionButton));

  // If the options page hasn't already come up, wait for it.
  if (tab_strip->count() == 1) {
    ui_test_utils::WaitForNewTab(browser());
  }
  ASSERT_EQ(2, tab_strip->count());

  EXPECT_EQ(extension->GetResourceURL("options.html"),
            tab_strip->GetWebContentsAt(1)->GetURL());
}
