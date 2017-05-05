// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/common/content_switches.h"
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

  void SelectTab(const std::string& name) {
    ASSERT_TRUE(content::ExecuteScript(
        GetWebContents(),
        std::string("var data = {pageId: '") + name + "'};" +
            "uber.invokeMethodOnWindow(this, 'changeSelection', data);"));
  }

 private:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  DISALLOW_COPY_AND_ASSIGN(UberUIBrowserTest);
};

IN_PROC_BROWSER_TEST_F(UberUIBrowserTest, EnableMdExtensionsHidesExtensions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kMaterialDesignExtensions},
                                       {features::kMaterialDesignSettings});

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIUberFrameURL));
  SelectTab("settings");
  EXPECT_TRUE(GetJsBool("$('extensions').hidden"));
}

IN_PROC_BROWSER_TEST_F(UberUIBrowserTest, EnableMdSettingsHidesSettings) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kMaterialDesignSettings},
                                       {features::kMaterialDesignExtensions});

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIUberFrameURL));
  SelectTab("extensions");
  EXPECT_TRUE(GetJsBool("$('settings').hidden && $('help').hidden"));
}

IN_PROC_BROWSER_TEST_F(UberUIBrowserTest,
                       EnableSettingsWindowHidesSettingsAndHelp) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kMaterialDesignSettings);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableSettingsWindow);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIUberFrameURL));
  SelectTab("extensions");
  EXPECT_TRUE(GetJsBool("$('settings').hidden && $('help').hidden"));
}
