// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"

#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/uber/uber_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include <string>

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

class SignOutWaiter : public SigninManagerBase::Observer {
 public:
  explicit SignOutWaiter(SigninManagerBase* signin_manager)
      : seen_(false), running_(false), scoped_observer_(this) {
    scoped_observer_.Add(signin_manager);
  }
  ~SignOutWaiter() override {}

  void Wait() {
    if (seen_)
      return;

    running_ = true;
    message_loop_runner_ = new MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(seen_);
  }

  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override {
    seen_ = true;
    if (!running_)
      return;

    message_loop_runner_->Quit();
    running_ = false;
  }

 private:
  bool seen_;
  bool running_;
  ScopedObserver<SigninManagerBase, SignOutWaiter> scoped_observer_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

#if !defined(OS_CHROMEOS)
void RunClosureWhenProfileInitialized(const base::Closure& closure,
                                      Profile* profile,
                                      Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    closure.Run();
}
#endif

bool FrameHasSettingsSourceHost(content::RenderFrameHost* frame) {
  return frame->GetLastCommittedURL().DomainIs(
      chrome::kChromeUISettingsFrameHost);
}

}  // namespace

OptionsUIBrowserTest::OptionsUIBrowserTest() {
}

void OptionsUIBrowserTest::NavigateToSettings() {
  NavigateToSettingsSubpage("");
}

void OptionsUIBrowserTest::NavigateToSettingsSubpage(
    const std::string& sub_page) {
  const GURL& url = chrome::GetSettingsUrl(sub_page);
  ui_test_utils::NavigateToURLWithDisposition(browser(), url, CURRENT_TAB, 0);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(web_contents->GetWebUI());

  content::WebUIController* controller =
      web_contents->GetWebUI()->GetController();
#if !defined(OS_CHROMEOS)
  controller = static_cast<UberUI*>(controller)->
      GetSubpage(chrome::kChromeUISettingsFrameURL)->GetController();
#endif
  OptionsUI* options_ui = static_cast<OptionsUI*>(controller);

  // It is not possible to subscribe to the OnFinishedLoading event before the
  // call to NavigateToURL(), because the WebUI does not yet exist at that time.
  // However, it is safe to subscribe afterwards, because the event will always
  // be posted asynchronously to the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner(new MessageLoopRunner);
  scoped_ptr<OptionsUI::OnFinishedLoadingCallbackList::Subscription>
      subscription = options_ui->RegisterOnFinishedLoadingCallback(
          message_loop_runner->QuitClosure());
  message_loop_runner->Run();

  // The OnFinishedLoading event, which indicates that all WebUI initialization
  // methods have been called on the JS side, is temporally unrelated to whether
  // or not the WebContents considers itself to have finished loading. We want
  // to wait for this too, however, because, e.g. this is a sufficient condition
  // to get the focus properly placed on a form element.
  content::WaitForLoadStop(web_contents);
}

void OptionsUIBrowserTest::NavigateToSettingsFrame() {
  const GURL& url = GURL(chrome::kChromeUISettingsFrameURL);
  ui_test_utils::NavigateToURL(browser(), url);
}

void OptionsUIBrowserTest::VerifyNavbar() {
  bool navbar_exist = false;
#if defined(OS_CHROMEOS)
  bool should_navbar_exist = false;
#else
  bool should_navbar_exist = true;
#endif
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send("
      "    !!document.getElementById('navigation'))",
      &navbar_exist));
  EXPECT_EQ(should_navbar_exist, navbar_exist);
}

void OptionsUIBrowserTest::VerifyTitle() {
  base::string16 title =
      browser()->tab_strip_model()->GetActiveWebContents()->GetTitle();
  base::string16 expected_title = l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE);
  EXPECT_NE(title.find(expected_title), base::string16::npos);
}

content::RenderFrameHost* OptionsUIBrowserTest::GetSettingsFrame() {
  // NB: The utility function content::FrameHasSourceUrl can't be used because
  // the settings frame navigates itself to chrome://settings-frame/settings
  // to indicate that it's showing the top-level settings. Therefore, just
  // match the host.
  return content::FrameMatchingPredicate(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::Bind(&FrameHasSettingsSourceHost));
}

IN_PROC_BROWSER_TEST_F(OptionsUIBrowserTest, LoadOptionsByURL) {
  NavigateToSettings();
  VerifyTitle();
  VerifyNavbar();
}

// Flaky on Linux, Mac and Win: http://crbug.com/469113
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_VerifyManagedSignout DISABLED_VerifyManagedSignout
#else
#define MAYBE_VerifyManagedSignout VerifyManagedSignout
#endif

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(OptionsUIBrowserTest, MAYBE_VerifyManagedSignout) {
  SigninManager* signin =
      SigninManagerFactory::GetForProfile(browser()->profile());
  signin->OnExternalSigninCompleted("test@example.com");
  signin->ProhibitSignout(true);

  NavigateToSettingsFrame();

  // This script simulates a click on the "Disconnect your Google Account"
  // button and returns true if the hidden flag of the appropriate dialog gets
  // flipped.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "var dialog = $('manage-profile-overlay-disconnect-managed');"
      "var original_status = dialog.hidden;"
      "var original = ManageProfileOverlay.showDisconnectManagedProfileDialog;"
      "var teststub = function(event) {"
      "  original(event);"
      "  domAutomationController.send(original_status && !dialog.hidden);"
      "};"
      "ManageProfileOverlay.showDisconnectManagedProfileDialog = teststub;"
      "$('start-stop-sync').click();",
      &result));

  EXPECT_TRUE(result);

  base::FilePath profile_dir = browser()->profile()->GetPath();
  ProfileInfoCache& profile_info_cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();

  EXPECT_TRUE(DirectoryExists(profile_dir));
  EXPECT_TRUE(profile_info_cache.GetIndexOfProfileWithPath(profile_dir) !=
              std::string::npos);

  // TODO(kaliamoorthi): Get the macos problem fixed and remove this code.
  // Deleting the Profile also destroys all browser windows of that Profile.
  // Wait for the current browser to close before resuming, otherwise
  // the browser_tests shutdown code will be confused on the Mac.
  content::WindowedNotificationObserver wait_for_browser_closed(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::NotificationService::AllSources());

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "$('disconnect-managed-profile-ok').click();"));

  EXPECT_TRUE(profile_info_cache.GetIndexOfProfileWithPath(profile_dir) ==
              std::string::npos);

  wait_for_browser_closed.Wait();
}

IN_PROC_BROWSER_TEST_F(OptionsUIBrowserTest, VerifyUnmanagedSignout) {
  const std::string user = "test@example.com";
  AccountTrackerServiceFactory::GetForProfile(browser()->profile())
      ->SeedAccountInfo("12345", user);
  SigninManager* signin =
      SigninManagerFactory::GetForProfile(browser()->profile());
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

  SignOutWaiter sign_out_waiter(signin);

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "$('stop-syncing-ok').click();"));

  sign_out_waiter.Wait();

  EXPECT_TRUE(browser()->profile()->GetProfileUserName() != user);
  EXPECT_FALSE(signin->IsAuthenticated());
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
                 std::string(),
                 std::string());
  run_loop.Run();

  // Verify that the settings page has updated and lists two profiles.
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      contents, javascript, &profiles));
  EXPECT_EQ(2, profiles);
}
#endif

}  // namespace options
