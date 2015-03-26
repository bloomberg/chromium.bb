// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

class UberUIBrowserTest : public WebUIBrowserTest {};

IN_PROC_BROWSER_TEST_F(UberUIBrowserTest, HistoryOverride) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIUberFrameURL));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Inject script. This script will be called when Extension is loaded.
  const std::string inject_script =
      "var is_called_override = false;"
      "var uber_frame = {};"
      "uber_frame.setNavigationOverride = function(controls, override) { "
      "is_called_override = true; };";

  ASSERT_TRUE(content::ExecuteScript(tab, inject_script));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "History Override")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Set("permission",
                                extensions::ListBuilder().Append("history")))
          .Build();

  ExtensionService* service = extensions::ExtensionSystem::Get(
                                  browser()->profile())->extension_service();
  // Load extension. UberUI overrides history navigation.
  // In this test, injected script will be called instead.
  service->AddExtension(extension.get());

  bool called_override;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "domAutomationController.send(is_called_override);",
      &called_override));
  EXPECT_TRUE(called_override);
}
