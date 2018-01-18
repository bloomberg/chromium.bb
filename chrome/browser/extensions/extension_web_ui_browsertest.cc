// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using ExtensionWebUIBrowserTest = ExtensionBrowserTest;

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionWebUIBrowserTest,
                       NavigateToDefaultChromePageOnExtensionUnload) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("override")
                        .AppendASCII("history"));
  ASSERT_TRUE(extension);
  std::string extension_id = extension->id();

  // Open the chrome://history/ page in the current tab.
  GURL extension_url(chrome::kChromeUIHistoryURL);
  ui_test_utils::NavigateToURL(browser(), extension_url);
  content::WaitForLoadStop(tab_strip_model->GetActiveWebContents());

  // Check that the chrome://history/ page contains the extension's content.
  std::string location;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send(location.href);\n", &location));
  EXPECT_EQ(extension_id, GURL(location).host());  // Extension has control.

  ASSERT_EQ(1, tab_strip_model->count());

  // Uninstall the extension.
  ExtensionService* service =
      ExtensionSystem::Get(browser()->profile())->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  TestExtensionRegistryObserver registry_observer(registry);
  service->UnloadExtension(extension->id(), UnloadedExtensionReason::UNINSTALL);
  registry_observer.WaitForExtensionUnloaded();

  // Check that the opened chrome://history/ page contains the default chrome
  // history page content.
  ASSERT_EQ(1, tab_strip_model->count());
  content::WaitForLoadStop(tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(
      chrome::kChromeUIHistoryURL,
      tab_strip_model->GetActiveWebContents()->GetLastCommittedURL().spec());

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_strip_model->GetActiveWebContents(),
      "domAutomationController.send(location.href);\n", &location));
  // Default history page has control.
  EXPECT_EQ(chrome::kChromeUIHistoryURL, GURL(location).spec());
}

}  // namespace extensions
