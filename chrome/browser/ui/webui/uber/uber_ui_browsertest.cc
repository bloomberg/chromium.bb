// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

class UberUIBrowserTest : public WebUIBrowserTest {
 public:
  UberUIBrowserTest() {}
  ~UberUIBrowserTest() override {}

  bool GetJsBool(const char* js) {
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetWebContents(),
        std::string("domAutomationController.send(") + js + ");",
        &result));
    return result;
  }

  void RunJs(const char* js) {
    ASSERT_TRUE(content::ExecuteScript(GetWebContents(), js));
  }

  void SelectTab() {
    RunJs("var data = {pageId: 'history'};"
          "uber.invokeMethodOnWindow(this, 'changeSelection', data);");
  }

 private:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  DISALLOW_COPY_AND_ASSIGN(UberUIBrowserTest);
};

IN_PROC_BROWSER_TEST_F(UberUIBrowserTest, HistoryOverride) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIUberFrameURL));

  RunJs("var overrideCalled = false;"
        "var uber_frame = {"
        "  setNavigationOverride: function() {"
        "    overrideCalled = true;"
        "  },"
        "};");

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

  EXPECT_TRUE(GetJsBool("overrideCalled"));
}

IN_PROC_BROWSER_TEST_F(UberUIBrowserTest, EnableMdExtensionsHidesExtensions) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableMaterialDesignExtensions);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIUberFrameURL));
  SelectTab();
  EXPECT_TRUE(GetJsBool("$('extensions').hidden"));
}

IN_PROC_BROWSER_TEST_F(UberUIBrowserTest,
                       EnableSettingsWindowHidesSettingsAndHelp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableSettingsWindow);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIUberFrameURL));
  SelectTab();
  EXPECT_TRUE(GetJsBool("$('settings').hidden && $('help').hidden"));
}
