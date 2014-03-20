// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

using extensions::Extension;

// Used to simulate a click on the first element named 'Options'.
static const char kScriptClickOptionButton[] =
    "(function() { "
    "  var button = document.querySelector('.options-link');"
    "  button.click();"
    "})();";

// Test that an extension with an options page makes an 'Options' button appear
// on chrome://extensions, and that clicking the button opens a new tab with the
// extension's options page.
// Disabled because of flakiness. See http://crbug.com/174934.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, DISABLED_OptionsPage) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  size_t installed_extensions = service->extensions()->size();
  // Install an extension with an options page.
  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("options.crx"), 1);
  ASSERT_TRUE(extension);
  EXPECT_EQ(installed_extensions + 1, service->extensions()->size());

  // Go to the Extension Settings page and click the Options button.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIExtensionsURL));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ui_test_utils::WindowedTabAddedNotificationObserver observer(
      content::NotificationService::AllSources());
  // NOTE: Currently the above script needs to execute in an iframe. The
  // selector for that iframe may break if the layout of the extensions
  // page changes.
  content::RenderFrameHost* frame = content::FrameMatchingPredicate(
      tab_strip->GetActiveWebContents(),
      base::Bind(&content::FrameHasSourceUrl,
                 GURL(chrome::kChromeUIExtensionsFrameURL)));
  EXPECT_TRUE(content::ExecuteScript(
      frame,
      kScriptClickOptionButton));
  observer.Wait();
  EXPECT_EQ(2, tab_strip->count());

  EXPECT_EQ(extension->GetResourceURL("options.html"),
            tab_strip->GetWebContentsAt(1)->GetURL());
}
