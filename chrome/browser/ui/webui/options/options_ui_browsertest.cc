// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#endif

namespace options {

namespace {

#if !defined(OS_CHROMEOS)
void RunClosureWhenProfileInitialized(const base::Closure& closure,
                                      Profile* profile,
                                      Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    closure.Run();
}
#endif

}  // namespace

OptionsUIBrowserTest::OptionsUIBrowserTest() {
}

void OptionsUIBrowserTest::NavigateToSettings() {
  const GURL& url = GURL(chrome::kChromeUISettingsURL);
  ui_test_utils::NavigateToURL(browser(), url);
}

void OptionsUIBrowserTest::VerifyNavbar() {
  bool navbar_exist = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send("
      "    !!document.getElementById('navigation'))",
      &navbar_exist));
  EXPECT_EQ(true, navbar_exist);
}

void OptionsUIBrowserTest::VerifyTitle() {
  string16 title =
      browser()->tab_strip_model()->GetActiveWebContents()->GetTitle();
  string16 expected_title = l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE);
  EXPECT_NE(title.find(expected_title), string16::npos);
}

// Flaky, see http://crbug.com/119671.
IN_PROC_BROWSER_TEST_F(OptionsUIBrowserTest, DISABLED_LoadOptionsByURL) {
  NavigateToSettings();
  VerifyTitle();
  VerifyNavbar();
}

#if !defined(OS_CHROMEOS)
// Regression test for http://crbug.com/301436, excluded on Chrome OS because
// profile management in the settings UI exists on desktop platforms only.
IN_PROC_BROWSER_TEST_F(OptionsUIBrowserTest, NavigateBackFromOverlayDialog) {
  // Navigate to the settings page.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings-frame"));

  // Click a button that opens an overlay dialog.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      contents, "$('manage-default-search-engines').click();"));

  // Go back to the settings page.
  content::TestNavigationObserver observer(contents);
  chrome::GoBack(browser(), CURRENT_TAB);
  observer.Wait();

  // Verify that the settings page lists one profile.
  const char javascript[] =
      "domAutomationController.send("
      "    document.querySelectorAll('list#profiles-list > div[role=listitem]')"
      "        .length);";
  int profiles;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      contents, javascript, &profiles));
  EXPECT_EQ(1, profiles);

  // Create a second profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();

  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      profile_manager->GenerateNextProfileDirectoryPath(),
      base::Bind(&RunClosureWhenProfileInitialized,
                 run_loop.QuitClosure()),
                 string16(),
                 string16(),
                 std::string());
  run_loop.Run();

  // Verify that the settings page has updated and lists two profiles.
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      contents, javascript, &profiles));
  EXPECT_EQ(2, profiles);
}
#endif

}  // namespace options
