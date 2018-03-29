// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/macros.h"
#include "build/buildflag.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_ui_util {

class GetAllowedDomainTest : public ::testing::Test {};

TEST_F(GetAllowedDomainTest, WithInvalidPattern) {
  EXPECT_EQ(std::string(), GetAllowedDomain("email"));
  EXPECT_EQ(std::string(), GetAllowedDomain("email@a@b"));
  EXPECT_EQ(std::string(), GetAllowedDomain("email@a[b"));
  EXPECT_EQ(std::string(), GetAllowedDomain("@$"));
  EXPECT_EQ(std::string(), GetAllowedDomain("@\\E$"));
  EXPECT_EQ(std::string(), GetAllowedDomain("@\\E$a"));
  EXPECT_EQ(std::string(), GetAllowedDomain("email@"));
  EXPECT_EQ(std::string(), GetAllowedDomain("@"));
  EXPECT_EQ(std::string(), GetAllowedDomain("example@a.com|example@b.com"));
  EXPECT_EQ(std::string(), GetAllowedDomain(""));
}

TEST_F(GetAllowedDomainTest, WithValidPattern) {
  EXPECT_EQ("example.com", GetAllowedDomain("email@example.com"));
  EXPECT_EQ("example.com", GetAllowedDomain("email@example.com\\E"));
  EXPECT_EQ("example.com", GetAllowedDomain("email@example.com$"));
  EXPECT_EQ("example.com", GetAllowedDomain("email@example.com\\E$"));
  EXPECT_EQ("example.com", GetAllowedDomain("*@example.com\\E$"));
  EXPECT_EQ("example.com", GetAllowedDomain(".*@example.com\\E$"));
  EXPECT_EQ("example-1.com", GetAllowedDomain("email@example-1.com"));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {
const char kMainEmail[] = "main_email@example.com";
const char kMainGaiaID[] = "main_gaia_id";
class SigninUiUtilTestBrowserWindow : public TestBrowserWindow {
 public:
  SigninUiUtilTestBrowserWindow() = default;
  ~SigninUiUtilTestBrowserWindow() override = default;
  void set_browser(Browser* browser) { browser_ = browser; }

  void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      bool is_source_keyboard) override {
    ASSERT_TRUE(browser_);
    // Simulate what |BrowserView| does for a regular Chrome sign-in flow.
    browser_->signin_view_controller()->ShowSignin(
        profiles::BubbleViewMode::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser_,
        access_point);
  }

 private:
  Browser* browser_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SigninUiUtilTestBrowserWindow);
};
}  // namespace

class DiceSigninUiUtilTest : public BrowserWithTestWindowTest {
 public:
  DiceSigninUiUtilTest()
      : scoped_account_consistency_(signin::AccountConsistencyMethod::kDice) {}
  ~DiceSigninUiUtilTest() override = default;

  struct CreateDiceTurnSyncOnHelperParams {
   public:
    Profile* profile = nullptr;
    Browser* browser = nullptr;
    signin_metrics::AccessPoint signin_access_point =
        signin_metrics::AccessPoint::ACCESS_POINT_MAX;
    signin_metrics::Reason signin_reason = signin_metrics::Reason::REASON_MAX;
    std::string account_id;
    DiceTurnSyncOnHelper::SigninAbortedMode signin_aborted_mode =
        DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT;
  };

  void CreateDiceTurnSyncOnHelper(
      Profile* profile,
      Browser* browser,
      signin_metrics::AccessPoint signin_access_point,
      signin_metrics::Reason signin_reason,
      const std::string& account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode signin_aborted_mode) {
    create_dice_turn_sync_on_helper_called_ = true;
    create_dice_turn_sync_on_helper_params_.profile = profile;
    create_dice_turn_sync_on_helper_params_.browser = browser;
    create_dice_turn_sync_on_helper_params_.signin_access_point =
        signin_access_point;
    create_dice_turn_sync_on_helper_params_.signin_reason = signin_reason;
    create_dice_turn_sync_on_helper_params_.account_id = account_id;
    create_dice_turn_sync_on_helper_params_.signin_aborted_mode =
        signin_aborted_mode;
  }

 protected:
  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    static_cast<SigninUiUtilTestBrowserWindow*>(browser()->window())
        ->set_browser(browser());
  }

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{SigninManagerFactory::GetInstance(), BuildFakeSigninManagerBase},
            {ProfileOAuth2TokenServiceFactory::GetInstance(),
             BuildFakeProfileOAuth2TokenService},
            {GaiaCookieManagerServiceFactory::GetInstance(),
             BuildFakeGaiaCookieManagerService}};
  }

  // BrowserWithTestWindowTest:
  BrowserWindow* CreateBrowserWindow() override {
    return new SigninUiUtilTestBrowserWindow();
  }

  // Returns the token service.
  ProfileOAuth2TokenService* GetTokenService() {
    return ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
  }

  // Returns the account tracker service.
  AccountTrackerService* GetAccountTrackerService() {
    return AccountTrackerServiceFactory::GetForProfile(profile());
  }

  void EnableSync(const AccountInfo& account_info) {
    signin_ui_util::internal::EnableSync(
        browser(), account_info,
        signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
        base::BindOnce(&DiceSigninUiUtilTest::CreateDiceTurnSyncOnHelper,
                       base::Unretained(this)));
  }

  signin::ScopedAccountConsistency scoped_account_consistency_;
  bool create_dice_turn_sync_on_helper_called_ = false;
  CreateDiceTurnSyncOnHelperParams create_dice_turn_sync_on_helper_params_;
};

TEST_F(DiceSigninUiUtilTest, EnableSyncWithExistingAccount) {
  // Add an account.
  std::string account_id =
      GetAccountTrackerService()->SeedAccountInfo(kMainEmail, kMainGaiaID);
  GetTokenService()->UpdateCredentials(account_id, "token");

  EnableSync(GetAccountTrackerService()->GetAccountInfo(account_id));
  ASSERT_TRUE(create_dice_turn_sync_on_helper_called_);

  // Verify that the helper to enable sync is created with the expected params.
  EXPECT_EQ(profile(), create_dice_turn_sync_on_helper_params_.profile);
  EXPECT_EQ(browser(), create_dice_turn_sync_on_helper_params_.browser);
  EXPECT_EQ(account_id, create_dice_turn_sync_on_helper_params_.account_id);
  EXPECT_EQ(signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
            create_dice_turn_sync_on_helper_params_.signin_access_point);
  EXPECT_EQ(signin_metrics::Reason::REASON_UNKNOWN_REASON,
            create_dice_turn_sync_on_helper_params_.signin_reason);
  EXPECT_EQ(DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
            create_dice_turn_sync_on_helper_params_.signin_aborted_mode);
}

TEST_F(DiceSigninUiUtilTest, EnableSyncWithAccountThatNeedsReauth) {
  // Add an account to the account tracker, but do not add it to the token
  // service in order for it to require a reauth before enabling sync.
  std::string account_id =
      GetAccountTrackerService()->SeedAccountInfo(kMainGaiaID, kMainEmail);

  EnableSync(GetAccountTrackerService()->GetAccountInfo(account_id));
  ASSERT_FALSE(create_dice_turn_sync_on_helper_called_);

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetSigninURLForDice(profile(), kMainEmail),
            active_contents->GetVisibleURL());
}

TEST_F(DiceSigninUiUtilTest, EnableSyncForNewAccountWithNoTab) {
  EnableSync(AccountInfo());
  ASSERT_FALSE(create_dice_turn_sync_on_helper_called_);

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetSigninURLForDice(profile(), ""),
            active_contents->GetVisibleURL());
}

TEST_F(DiceSigninUiUtilTest, EnableSyncForNewAccountWithOneTab) {
  AddTab(browser(), GURL("http://foo/1"));

  EnableSync(AccountInfo());
  ASSERT_FALSE(create_dice_turn_sync_on_helper_called_);

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetSigninURLForDice(profile(), ""),
            active_contents->GetVisibleURL());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin_ui_util
