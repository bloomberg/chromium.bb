// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef InProcessBrowserTest FirstRunBrowserTest;

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShowFirstRunBubblePref) {
  EXPECT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShowFirstRunBubbleOption));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_DONT_SHOW,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(
      first_run::FIRST_RUN_BUBBLE_SHOW));
  ASSERT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShowFirstRunBubbleOption));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_SHOW,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
  // Test that toggling the value works in either direction after it's been set.
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(
      first_run::FIRST_RUN_BUBBLE_DONT_SHOW));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_DONT_SHOW,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
  // Test that the value can't be set to FIRST_RUN_BUBBLE_SHOW after it has been
  // set to FIRST_RUN_BUBBLE_SUPPRESS.
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(
      first_run::FIRST_RUN_BUBBLE_SUPPRESS));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_SUPPRESS,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(
      first_run::FIRST_RUN_BUBBLE_SHOW));
  EXPECT_EQ(first_run::FIRST_RUN_BUBBLE_SUPPRESS,
            g_browser_process->local_state()->GetInteger(
                prefs::kShowFirstRunBubbleOption));
}

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShouldShowWelcomePage) {
  EXPECT_FALSE(first_run::ShouldShowWelcomePage());
  first_run::SetShouldShowWelcomePage();
  EXPECT_TRUE(first_run::ShouldShowWelcomePage());
  EXPECT_FALSE(first_run::ShouldShowWelcomePage());
}

#if !defined(OS_CHROMEOS)
namespace {
class FirstRunIntegrationBrowserTest : public InProcessBrowserTest {
 public:
  FirstRunIntegrationBrowserTest() {}
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceFirstRun);
    EXPECT_FALSE(ProfileManager::DidPerformProfileImport());

    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    // The forked import process should run BrowserMain.
    CommandLine import_arguments((CommandLine::NoProgram()));
    import_arguments.AppendSwitch(content::kLaunchAsBrowser);
    first_run::SetExtraArgumentsForImportProcess(import_arguments);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunIntegrationBrowserTest);
};

class FirstRunMasterPrefsBrowserTest : public FirstRunIntegrationBrowserTest {
 public:
  FirstRunMasterPrefsBrowserTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(file_util::CreateTemporaryFile(&prefs_file_));
    // TODO(tapted): Make this reusable.
    const char text[] =
        "{\n"
        "  \"distribution\": {\n"
        "    \"import_bookmarks\": false,\n"
        "    \"import_history\": false,\n"
        "    \"import_home_page\": false,\n"
        "    \"import_search_engine\": false\n"
        "  }\n"
        "}\n";
    EXPECT_TRUE(file_util::WriteFile(prefs_file_, text, strlen(text)));
    first_run::SetMasterPrefsPathForTesting(prefs_file_);

    // This invokes BrowserMain, and does the import, so must be done last.
    FirstRunIntegrationBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(file_util::Delete(prefs_file_, false));
    FirstRunIntegrationBrowserTest::TearDown();
  }

 private:
  base::FilePath prefs_file_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunMasterPrefsBrowserTest);
};
}

IN_PROC_BROWSER_TEST_F(FirstRunIntegrationBrowserTest, WaitForImport) {
  ASSERT_TRUE(ProfileManager::DidPerformProfileImport());
}

// Test an import with all import options disabled. This is a regression test
// for http://crbug.com/169984 where this would cause the import process to
// stay running, and the NTP to be loaded with no apps.
IN_PROC_BROWSER_TEST_F(FirstRunMasterPrefsBrowserTest,
                       ImportNothingAndShowNewTabPage) {
  ASSERT_TRUE(ProfileManager::DidPerformProfileImport());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(1, tab->GetMaxPageID());
}

#endif  // !defined(OS_CHROMEOS)
