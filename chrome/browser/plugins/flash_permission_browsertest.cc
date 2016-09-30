// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/permissions/permissions_browsertest.h"
#include "chrome/browser/ui/website_settings/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/ppapi_test_utils.h"

class FlashPermissionBrowserTest : public PermissionsBrowserTest {
 public:
  FlashPermissionBrowserTest()
      : PermissionsBrowserTest("/permissions/flash.html") {}
  ~FlashPermissionBrowserTest() override {}

  // PermissionsBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PermissionsBrowserTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(ppapi::RegisterFlashTestPlugin(command_line));

    feature_list_.InitAndEnableFeature(features::kPreferHtmlOverPlugins);
  }

  void SetUpOnMainThread() override {
    SiteEngagementScore::SetParamValuesForTesting();

    PermissionsBrowserTest::SetUpOnMainThread();
  }
  void TriggerPrompt() override {
    EXPECT_TRUE(RunScriptReturnBool("triggerPrompt();"));
  }
  bool FeatureUsageSucceeds() override {
    // Flash won't be enabled until the page is refreshed.
    ui_test_utils::NavigateToURL(browser(),
                                 GetWebContents()->GetLastCommittedURL());
    return RunScriptReturnBool("flashIsEnabled();");
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest,
                       CommonFailsBeforeRequesting) {
  CommonFailsBeforeRequesting();
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, CommonFailsIfDismissed) {
  CommonFailsIfDismissed();
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, CommonFailsIfBlocked) {
  CommonFailsIfBlocked();
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, CommonSucceedsIfAllowed) {
  CommonSucceedsIfAllowed();
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, TriggerPromptViaNewWindow) {
  EXPECT_EQ(0, prompt_factory()->total_request_count());
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(RunScriptReturnBool("triggerPromptViaNewWindow();"));

  EXPECT_EQ(1, prompt_factory()->total_request_count());
  EXPECT_TRUE(FeatureUsageSucceeds());
}
