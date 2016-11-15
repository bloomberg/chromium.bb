// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/permissions/permissions_browsertest.h"
#include "chrome/browser/ui/website_settings/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/ppapi_test_utils.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "url/gurl.h"

class FlashPermissionBrowserTest : public PermissionsBrowserTest {
 public:
  FlashPermissionBrowserTest()
      : PermissionsBrowserTest("/permissions/flash.html") {}
  ~FlashPermissionBrowserTest() override {}

  // PermissionsBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PermissionsBrowserTest::SetUpCommandLine(command_line);

    ASSERT_TRUE(ppapi::RegisterFlashTestPlugin(command_line));

    // Set a high engagement threshhold so it doesn't interfere with testing the
    // permission.
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    "PreferHtmlOverPlugins<Study1");
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    "Study1/Enabled/");
    command_line->AppendSwitchASCII(
        variations::switches::kForceFieldTrialParams,
        "Study1.Enabled:engagement_threshold_for_flash/100");
  }

  void TriggerPrompt() override {
    if (prompt_factory()->response_type() ==
        PermissionRequestManager::ACCEPT_ALL) {
      // If the prompt will be allowed, we need to wait for the page to refresh.
      content::TestNavigationManager observer(
          GetWebContents(), GetWebContents()->GetLastCommittedURL());
      EXPECT_TRUE(RunScriptReturnBool("triggerPrompt();"));
      observer.WaitForNavigationFinished();
    } else {
      EXPECT_TRUE(RunScriptReturnBool("triggerPrompt();"));
    }
  }

  bool FeatureUsageSucceeds() override {
    // Wait until the page is refreshed before testing whether flash is enabled
    // or disabled.
    ui_test_utils::NavigateToURL(browser(),
                                 GetWebContents()->GetLastCommittedURL());
    // If either flash with or without fallback content runs successfully it
    // indicates the feature is at least partly working, which could imply a
    // faulty permission.
    return RunScriptReturnBool("flashIsEnabled();") ||
           RunScriptReturnBool("flashIsEnabledForPluginWithoutFallback();");
  }
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

  EXPECT_TRUE(FeatureUsageSucceeds());
  EXPECT_EQ(1, prompt_factory()->total_request_count());
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest,
                       TriggerPromptViaPluginPlaceholder) {
  EXPECT_EQ(0, prompt_factory()->total_request_count());
  EXPECT_FALSE(FeatureUsageSucceeds());
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  // We need to simulate a mouse click to trigger the placeholder to prompt.
  content::TestNavigationManager observer(
      GetWebContents(), GetWebContents()->GetLastCommittedURL());
  content::SimulateMouseClickAt(GetWebContents(), 0 /* modifiers */,
                                blink::WebMouseEvent::Button::Left,
                                gfx::Point(50, 50));
  observer.WaitForNavigationFinished();

  EXPECT_TRUE(FeatureUsageSucceeds());
  EXPECT_EQ(1, prompt_factory()->total_request_count());
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, AllowFileURL) {
  base::FilePath test_path;
  PathService::Get(chrome::DIR_TEST_DATA, &test_path);
  ui_test_utils::NavigateToURL(
      browser(), GURL("file://" + test_path.AsUTF8Unsafe() + test_url()));
  CommonSucceedsIfAllowed();
  EXPECT_EQ(1, prompt_factory()->total_request_count());

  // Navigate to a second URL to verify it's allowed on all file: URLs.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("file://" + test_path.AsUTF8Unsafe() + "/permissions/flash2.html"));
  EXPECT_TRUE(FeatureUsageSucceeds());
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, BlockFileURL) {
  base::FilePath test_path;
  PathService::Get(chrome::DIR_TEST_DATA, &test_path);
  ui_test_utils::NavigateToURL(
      browser(), GURL("file://" + test_path.AsUTF8Unsafe() + test_url()));
  CommonFailsIfBlocked();
  EXPECT_EQ(1, prompt_factory()->total_request_count());

  // Navigate to a second URL to verify it's blocked on all file: URLs.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("file://" + test_path.AsUTF8Unsafe() + "/permissions/flash2.html"));
  EXPECT_FALSE(FeatureUsageSucceeds());
}
