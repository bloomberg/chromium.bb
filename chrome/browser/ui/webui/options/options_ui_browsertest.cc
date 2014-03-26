// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/uber/uber_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
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

using content::MessageLoopRunner;

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
  ui_test_utils::NavigateToURLWithDisposition(browser(), url, CURRENT_TAB, 0);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(web_contents->GetWebUI());
  UberUI* uber_ui = static_cast<UberUI*>(
      web_contents->GetWebUI()->GetController());
  OptionsUI* options_ui = static_cast<OptionsUI*>(
      uber_ui->GetSubpage(chrome::kChromeUISettingsFrameURL)->GetController());

  // It is not possible to subscribe to the OnFinishedLoading event before the
  // call to NavigateToURL(), because the WebUI does not yet exist at that time.
  // However, it is safe to subscribe afterwards, because the event will always
  // be posted asynchronously to the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner(new MessageLoopRunner);
  scoped_ptr<OptionsUI::OnFinishedLoadingCallbackList::Subscription>
      subscription = options_ui->RegisterOnFinishedLoadingCallback(
          message_loop_runner->QuitClosure());
  message_loop_runner->Run();
}

void OptionsUIBrowserTest::NavigateToSettingsFrame() {
  const GURL& url = GURL(chrome::kChromeUISettingsFrameURL);
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
  base::string16 title =
      browser()->tab_strip_model()->GetActiveWebContents()->GetTitle();
  base::string16 expected_title = l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE);
  EXPECT_NE(title.find(expected_title), base::string16::npos);
}

IN_PROC_BROWSER_TEST_F(OptionsUIBrowserTest, LoadOptionsByURL) {
  NavigateToSettings();
  VerifyTitle();
  VerifyNavbar();
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(OptionsUIBrowserTest, VerifyUnmanagedSignout) {
  SigninManager* signin =
      SigninManagerFactory::GetForProfile(browser()->profile());
  const std::string user = "test@example.com";
  signin->OnExternalSigninCompleted(user);

  NavigateToSettingsFrame();

  // This script simulates a click on the "Disconnect your Google Account"
  // button and returns true if the hidden flag of the appropriate dialog gets
  // flipped.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "var dialog = $('sync-setup-stop-syncing');"
      "var original_status = dialog.hidden;"
      "$('start-stop-sync').click();"
      "domAutomationController.send(original_status && !dialog.hidden);",
      &result));

  EXPECT_TRUE(result);

  content::WindowedNotificationObserver wait_for_signout(
      chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
      content::NotificationService::AllSources());

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "$('stop-syncing-ok').click();"));

  wait_for_signout.Wait();

  EXPECT_TRUE(browser()->profile()->GetProfileName() != user);
  EXPECT_TRUE(signin->GetAuthenticatedUsername().empty());
}

// Regression test for http://crbug.com/301436, excluded on Chrome OS because
// profile management in the settings UI exists on desktop platforms only.
IN_PROC_BROWSER_TEST_F(OptionsUIBrowserTest, NavigateBackFromOverlayDialog) {
  NavigateToSettingsFrame();

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
                 base::string16(),
                 base::string16(),
                 std::string());
  run_loop.Run();

  // Verify that the settings page has updated and lists two profiles.
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      contents, javascript, &profiles));
  EXPECT_EQ(2, profiles);
}
#endif

}  // namespace options
