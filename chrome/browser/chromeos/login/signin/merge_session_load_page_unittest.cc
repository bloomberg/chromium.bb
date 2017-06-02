// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/signin/merge_session_load_page.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::InterstitialPage;
using content::WebContents;
using content::WebContentsTester;

namespace {

const char kURL1[] = "http://www.google.com/";
const char kURL2[] = "http://mail.google.com/";

const int64_t kSessionMergeTimeout = 60;

}  // namespace

namespace chromeos {

// An MergeSessionLoadPage class that does not create windows.
class TestMergeSessionLoadPage :  public MergeSessionLoadPage {
 public:
  TestMergeSessionLoadPage(WebContents* web_contents, const GURL& url)
      : MergeSessionLoadPage(
            web_contents,
            url,
            merge_session_throttling_utils::CompletionCallback()) {
    interstitial_page_->DontCreateViewForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMergeSessionLoadPage);
};

class MergeSessionLoadPageTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined OS_CHROMEOS
  test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
  }

  void TearDown() override {
#if defined OS_CHROMEOS
    // Clean up pending tasks that might depend on the user manager.
    base::RunLoop().RunUntilIdle();
    test_user_manager_.reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void Navigate(const char* url,
                int nav_entry_id,
                bool did_create_new_entry) {
    WebContentsTester::For(web_contents())
        ->TestDidNavigate(web_contents()->GetMainFrame(), nav_entry_id,
                          did_create_new_entry, GURL(url),
                          ui::PAGE_TRANSITION_TYPED);
  }

  void ShowInterstitial(const char* url) {
    (new TestMergeSessionLoadPage(web_contents(), GURL(url)))->Show();
  }

  // Returns the MergeSessionLoadPage currently showing or NULL if none is
  // showing.
  InterstitialPage* GetMergeSessionLoadPage() {
    return InterstitialPage::GetInterstitialPage(web_contents());
  }

  OAuth2LoginManager* GetOAuth2LoginManager() {
    content::BrowserContext* browser_context =
        web_contents()->GetBrowserContext();
    if (!browser_context)
      return NULL;

    Profile* profile = Profile::FromBrowserContext(browser_context);
    if (!profile)
      return NULL;

    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
            profile);
    return login_manager;
  }

  void SetMergeSessionState(OAuth2LoginManager::SessionRestoreState state) {
    OAuth2LoginManager* login_manager = GetOAuth2LoginManager();
    ASSERT_TRUE(login_manager);
    login_manager->SetSessionRestoreState(state);
  }

  void SetSessionRestoreStart(const base::Time& time) {
    OAuth2LoginManager* login_manager = GetOAuth2LoginManager();
    ASSERT_TRUE(login_manager);
    login_manager->SetSessionRestoreStartForTesting(time);
  }

 private:
  ScopedTestDeviceSettingsService test_device_settings_service_;
  ScopedTestCrosSettings test_cros_settings_;
  std::unique_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
};

TEST_F(MergeSessionLoadPageTest, MergeSessionPageNotShown) {
  SetMergeSessionState(OAuth2LoginManager::SESSION_RESTORE_DONE);
  // Start a load.
  Navigate(kURL1, 0, true);
  // Load next page.
  controller().LoadURL(GURL(kURL2), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing an merge session interstitial page
  // to be shown.
  InterstitialPage* interstitial = GetMergeSessionLoadPage();
  EXPECT_FALSE(interstitial);
}

TEST_F(MergeSessionLoadPageTest, MergeSessionPageNotShownOnTimeout) {
  SetMergeSessionState(OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS);
  SetSessionRestoreStart(
      base::Time::Now() +
      base::TimeDelta::FromSeconds(kSessionMergeTimeout + 1));

  // Start a load.
  Navigate(kURL1, 0, true);
  // Load next page.
  controller().LoadURL(GURL(kURL2), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing an merge session interstitial page
  // to be shown.
  InterstitialPage* interstitial = GetMergeSessionLoadPage();
  EXPECT_FALSE(interstitial);
}

TEST_F(MergeSessionLoadPageTest, MergeSessionPageShown) {
  SetMergeSessionState(OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS);

  // Start a load.
  Navigate(kURL1, 0, true);
  // Load next page.
  controller().LoadURL(GURL(kURL2), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());
  int pending_id = controller().GetPendingEntry()->GetUniqueID();

  // Simulate the load causing an merge session interstitial page
  // to be shown.
  ShowInterstitial(kURL2);
  InterstitialPage* interstitial = GetMergeSessionLoadPage();
  ASSERT_TRUE(interstitial);
  base::RunLoop().RunUntilIdle();

  // Simulate merge session completion.
  SetMergeSessionState(OAuth2LoginManager::SESSION_RESTORE_DONE);
  base::RunLoop().RunUntilIdle();

  // The URL remains to be URL2.
  EXPECT_EQ(kURL2, web_contents()->GetVisibleURL().spec());

  // Commit navigation and the interstitial page is gone.
  Navigate(kURL2, pending_id, true);
  EXPECT_FALSE(GetMergeSessionLoadPage());
}

}  // namespace chromeos
