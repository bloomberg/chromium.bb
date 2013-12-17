// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/oobe_base_test.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"

namespace chromeos {

namespace {

// Email of owner account for test.
const char kTestAccountId[] = "username@gmail.com";

const char kTestAuthCode[] = "fake-auth-code";
const char kTestGaiaUberToken[] = "fake-uber-token";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";
const char kTestUserinfoToken[] = "fake-userinfo-token";
const char kTestLoginToken[] = "fake-login-token";
const char kTestSyncToken[] = "fake-sync-token";
const char kTestAuthLoginToken[] = "fake-oauthlogin-token";

}  // namespace

class OAuth2Test : public OobeBaseTest {
 protected:
  OAuth2Test() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    OobeBaseTest::SetUpOnMainThread();

    // Configure OAuth authentication.
    GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

    fake_gaia_.SetAuthTokens(kTestAuthCode,
                             kTestRefreshToken,
                             kTestAuthLoginAccessToken,
                             kTestGaiaUberToken,
                             kTestSessionSIDCookie,
                             kTestSessionLSIDCookie);
    // This token satisfies the userinfo.email request from
    // DeviceOAuth2TokenService used in token validation.
    FakeGaia::AccessTokenInfo userinfo_token_info;
    userinfo_token_info.token = kTestUserinfoToken;
    userinfo_token_info.scopes.insert(
        "https://www.googleapis.com/auth/userinfo.email");
    userinfo_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    userinfo_token_info.email = kTestAccountId;
    fake_gaia_.IssueOAuthToken(kTestRefreshToken, userinfo_token_info);

    FakeGaia::AccessTokenInfo userinfo_profile_token_info;
    userinfo_profile_token_info.token = kTestUserinfoToken;
    userinfo_profile_token_info.scopes.insert(
        "https://www.googleapis.com/auth/userinfo.profile");
    userinfo_profile_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    userinfo_profile_token_info.email = kTestAccountId;
    fake_gaia_.IssueOAuthToken(kTestRefreshToken, userinfo_profile_token_info);

    // The any-api access token for accessing the token minting endpoint.
    FakeGaia::AccessTokenInfo login_token_info;
    login_token_info.token = kTestLoginToken;
    login_token_info.scopes.insert(GaiaConstants::kAnyApiOAuth2Scope);
    login_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    fake_gaia_.IssueOAuthToken(kTestRefreshToken, login_token_info);

    // The /auth/chromesync access token for accessing sync endpoint.
    FakeGaia::AccessTokenInfo sync_token_info;
    sync_token_info.token = kTestSyncToken;
    sync_token_info.scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
    sync_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    fake_gaia_.IssueOAuthToken(kTestRefreshToken, sync_token_info);

    FakeGaia::AccessTokenInfo auth_login_token_info;
    auth_login_token_info.token = kTestAuthLoginToken;
    auth_login_token_info.scopes.insert(gaia_urls->oauth1_login_scope());
    auth_login_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    fake_gaia_.IssueOAuthToken(kTestRefreshToken, auth_login_token_info);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OAuth2Test);
};

class OAuth2LoginManagerStateWaiter : public OAuth2LoginManager::Observer {
 public:
  explicit OAuth2LoginManagerStateWaiter(Profile* profile)
     : profile_(profile),
       waiting_for_state_(false),
       final_state_(OAuth2LoginManager::SESSION_RESTORE_NOT_STARTED) {
  }

  void WaitForStates(
      const std::set<OAuth2LoginManager::SessionRestoreState>& states) {
    DCHECK(!waiting_for_state_);
    OAuth2LoginManager* login_manager =
         OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile_);
    states_ = states;
    if (states_.find(login_manager->state()) != states_.end()) {
      final_state_ = login_manager->state();
      return;
    }

    waiting_for_state_ = true;
    login_manager->AddObserver(this);
    runner_ = new content::MessageLoopRunner;
    runner_->Run();
    login_manager->RemoveObserver(this);
  }

  OAuth2LoginManager::SessionRestoreState final_state() { return final_state_; }

 private:
  // OAuth2LoginManager::Observer overrides.
  virtual void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) OVERRIDE {
    if (!waiting_for_state_)
      return;

    if (states_.find(state) == states_.end())
      return;

    final_state_ = state;
    waiting_for_state_ = false;
    runner_->Quit();
  }

  Profile* profile_;
  std::set<OAuth2LoginManager::SessionRestoreState> states_;
  bool waiting_for_state_;
  OAuth2LoginManager::SessionRestoreState final_state_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2LoginManagerStateWaiter);
};

IN_PROC_BROWSER_TEST_F(OAuth2Test, NewUser) {
  SimulateNetworkOnline();
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
    content::NotificationService::AllSources()).Wait();

  // Use capitalized and dotted user name on purpose to make sure
  // our email normalization kicks in.
  GetLoginDisplay()->ShowSigninScreenForCreds("User.Name", "password");

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_SESSION_STARTED,
    content::NotificationService::AllSources()).Wait();

  std::set<OAuth2LoginManager::SessionRestoreState> states;
  states.insert(OAuth2LoginManager::SESSION_RESTORE_DONE);
  states.insert(OAuth2LoginManager::SESSION_RESTORE_FAILED);
  states.insert(OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED);
  OAuth2LoginManagerStateWaiter merge_session_waiter(
      ProfileManager::GetPrimaryUserProfile());
  merge_session_waiter.WaitForStates(states);
  EXPECT_EQ(merge_session_waiter.final_state(),
            OAuth2LoginManager::SESSION_RESTORE_DONE);
}

}  // namespace chromeos
