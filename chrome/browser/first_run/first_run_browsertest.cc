// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
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

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShowWelcomePagePref) {
  EXPECT_FALSE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowWelcomePage));
  EXPECT_TRUE(first_run::SetShowWelcomePagePref());
  ASSERT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowWelcomePage));
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kShouldShowWelcomePage));
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
}

IN_PROC_BROWSER_TEST_F(FirstRunIntegrationBrowserTest, WaitForImport) {
  ASSERT_TRUE(ProfileManager::DidPerformProfileImport());
}
#endif  // !defined(OS_CHROMEOS)
