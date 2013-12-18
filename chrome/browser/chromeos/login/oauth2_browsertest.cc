// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/oobe_base_test.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

namespace {

// Email of owner account for test.
const char kTestAccountId[] = "username@gmail.com";
const char kTestRawAccountId[] = "User.Name";
const char kTestAccountPassword[] = "fake-password";
const char kTestAuthCode[] = "fake-auth-code";
const char kTestGaiaUberToken[] = "fake-uber-token";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestAuthSIDCookie[] = "fake-auth-SID-cookie";
const char kTestAuthLSIDCookie[] = "fake-auth-LSID-cookie";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";
const char kTestSession2SIDCookie[] = "fake-session2-SID-cookie";
const char kTestSession2LSIDCookie[] = "fake-session2-LSID-cookie";
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
  }

  void SetupGaiaServerForNewAccount() {
    FakeGaia::MergeSessionParams params;
    params.auth_sid_cookie = kTestAuthSIDCookie;
    params.auth_lsid_cookie = kTestAuthLSIDCookie;
    params.auth_code = kTestAuthCode;
    params.refresh_token = kTestRefreshToken;
    params.access_token = kTestAuthLoginAccessToken;
    params.gaia_uber_token = kTestGaiaUberToken;
    params.session_sid_cookie = kTestSessionSIDCookie;
    params.session_lsid_cookie = kTestSessionLSIDCookie;
    fake_gaia_.SetMergeSessionParams(params);
    SetupGaiaServerWithAccessTokens();
  }

  void SetupGaiaServerForExistingAccount() {
    FakeGaia::MergeSessionParams params;
    params.gaia_uber_token = kTestGaiaUberToken;
    params.session_sid_cookie = kTestSession2SIDCookie;
    params.session_lsid_cookie = kTestSession2LSIDCookie;
    fake_gaia_.SetMergeSessionParams(params);
    SetupGaiaServerWithAccessTokens();
  }

  bool TryToLogin(const std::string& username,
                  const std::string& password) {
    if (!AddUserTosession(username, password))
      return false;

    if (const User* active_user = UserManager::Get()->GetActiveUser())
      return active_user->email() == username;

    return false;
  }

  User::OAuthTokenStatus GetOAuthStatusFromLocalState(
      const std::string& user_id) const {
    PrefService* local_state = g_browser_process->local_state();
    const DictionaryValue* prefs_oauth_status =
        local_state->GetDictionary("OAuthTokenStatus");
    int oauth_token_status = User::OAUTH_TOKEN_STATUS_UNKNOWN;
    if (prefs_oauth_status &&
        prefs_oauth_status->GetIntegerWithoutPathExpansion(
            user_id, &oauth_token_status)) {
      User::OAuthTokenStatus result =
          static_cast<User::OAuthTokenStatus>(oauth_token_status);
      return result;
    }
    return User::OAUTH_TOKEN_STATUS_UNKNOWN;
  }

 private:
  bool AddUserTosession(const std::string& username,
                        const std::string& password) {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (!controller) {
      ADD_FAILURE();
      return false;
    }

    controller->Login(UserContext(username, password, std::string()));
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources()).Wait();
    const UserList& logged_users = UserManager::Get()->GetLoggedInUsers();
    for (UserList::const_iterator it = logged_users.begin();
         it != logged_users.end(); ++it) {
      if ((*it)->email() == username)
        return true;
    }
    return false;
  }

  void SetupGaiaServerWithAccessTokens() {
    // Configure OAuth authentication.
    GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

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

  DISALLOW_COPY_AND_ASSIGN(OAuth2Test);
};

class CookieReader : public base::RefCountedThreadSafe<CookieReader> {
 public:
  CookieReader() {
  }

  void ReadCookies(Profile* profile) {
    context_ = profile->GetRequestContext();
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&CookieReader::ReadCookiesOnIOThread,
                   this));
    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

  std::string GetCookieValue(const std::string& name) {
    for (std::vector<net::CanonicalCookie>::const_iterator iter =
             cookie_list_.begin();
        iter != cookie_list_.end();
        ++iter) {
      if (iter->Name() == name) {
        return iter->Value();
      }
    }
    return std::string();
  }

 private:
  friend class base::RefCountedThreadSafe<CookieReader>;

  virtual ~CookieReader() {
  }

  void ReadCookiesOnIOThread() {
    context_->GetURLRequestContext()->cookie_store()->GetCookieMonster()->
        GetAllCookiesAsync(base::Bind(
            &CookieReader::OnGetAllCookiesOnUIThread,
            this));
  }

  void OnGetAllCookiesOnUIThread(const net::CookieList& cookies) {
    cookie_list_ = cookies;
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&CookieReader::OnCookiesReadyOnUIThread,
                   this));
  }

  void OnCookiesReadyOnUIThread() {
    runner_->Quit();
  }

  scoped_refptr<net::URLRequestContextGetter> context_;
  net::CookieList cookie_list_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(CookieReader);
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

// PRE_MergeSession is testing merge session for a new profile.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_PRE_MergeSession) {
  SetupGaiaServerForNewAccount();
  SimulateNetworkOnline();
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
    content::NotificationService::AllSources()).Wait();

  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestAccountId),
            User::OAUTH_TOKEN_STATUS_UNKNOWN);

  // Use capitalized and dotted user name on purpose to make sure
  // our email normalization kicks in.
  GetLoginDisplay()->ShowSigninScreenForCreds(kTestRawAccountId,
                                              kTestAccountPassword);

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_SESSION_STARTED,
    content::NotificationService::AllSources()).Wait();
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  // Wait for the session merge to finish.
  std::set<OAuth2LoginManager::SessionRestoreState> states;
  states.insert(OAuth2LoginManager::SESSION_RESTORE_DONE);
  states.insert(OAuth2LoginManager::SESSION_RESTORE_FAILED);
  states.insert(OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED);
  OAuth2LoginManagerStateWaiter merge_session_waiter(
      ProfileManager::GetPrimaryUserProfile());
  merge_session_waiter.WaitForStates(states);
  EXPECT_EQ(merge_session_waiter.final_state(),
            OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Check for existance of refresh token.
  ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(
            profile);
  EXPECT_TRUE(token_service->RefreshTokenIsAvailable(kTestAccountId));

  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestAccountId),
            User::OAUTH2_TOKEN_STATUS_VALID);

  scoped_refptr<CookieReader> cookie_reader(new CookieReader());
  cookie_reader->ReadCookies(profile);
  EXPECT_EQ(cookie_reader->GetCookieValue("SID"), kTestSessionSIDCookie);
  EXPECT_EQ(cookie_reader->GetCookieValue("LSID"), kTestSessionLSIDCookie);
}

// MergeSession test is running merge session process for an existing profile
// that was generated in PRE_PRE_MergeSession test.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_MergeSession) {
  SetupGaiaServerForExistingAccount();
  SimulateNetworkOnline();

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
    content::NotificationService::AllSources()).Wait();

  JsExpect("!!document.querySelector('#account-picker')");
  JsExpect("!!document.querySelector('#pod-row')");

  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestAccountId),
            User::OAUTH2_TOKEN_STATUS_VALID);

  EXPECT_TRUE(TryToLogin(kTestAccountId, kTestAccountPassword));
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  // Wait for the session merge to finish.
  std::set<OAuth2LoginManager::SessionRestoreState> states;
  states.insert(OAuth2LoginManager::SESSION_RESTORE_DONE);
  states.insert(OAuth2LoginManager::SESSION_RESTORE_FAILED);
  states.insert(OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED);
  OAuth2LoginManagerStateWaiter merge_session_waiter(profile);
  merge_session_waiter.WaitForStates(states);
  EXPECT_EQ(merge_session_waiter.final_state(),
            OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Check for existance of refresh token.
  ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  EXPECT_TRUE(token_service->RefreshTokenIsAvailable(kTestAccountId));

  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestAccountId),
            User::OAUTH2_TOKEN_STATUS_VALID);

  scoped_refptr<CookieReader> cookie_reader(new CookieReader());
  cookie_reader->ReadCookies(profile);
  EXPECT_EQ(cookie_reader->GetCookieValue("SID"), kTestSession2SIDCookie);
  EXPECT_EQ(cookie_reader->GetCookieValue("LSID"), kTestSession2LSIDCookie);
}

// MergeSession test is attempting to merge session for an existing profile
// that was generated in PRE_PRE_MergeSession test. This attempt should fail
// since FakeGaia instance isn't configured to return relevant tokens/cookies.
IN_PROC_BROWSER_TEST_F(OAuth2Test, MergeSession) {
  SimulateNetworkOnline();

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
    content::NotificationService::AllSources()).Wait();

  JsExpect("!!document.querySelector('#account-picker')");
  JsExpect("!!document.querySelector('#pod-row')");

  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestAccountId),
            User::OAUTH2_TOKEN_STATUS_VALID);

  EXPECT_TRUE(TryToLogin(kTestAccountId, kTestAccountPassword));

  // Wait for the session merge to finish.
  std::set<OAuth2LoginManager::SessionRestoreState> states;
  states.insert(OAuth2LoginManager::SESSION_RESTORE_DONE);
  states.insert(OAuth2LoginManager::SESSION_RESTORE_FAILED);
  states.insert(OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED);
  OAuth2LoginManagerStateWaiter merge_session_waiter(
      ProfileManager::GetPrimaryUserProfile());
  merge_session_waiter.WaitForStates(states);
  EXPECT_EQ(merge_session_waiter.final_state(),
            OAuth2LoginManager::SESSION_RESTORE_FAILED);

  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestAccountId),
            User::OAUTH2_TOKEN_STATUS_INVALID);
}

}  // namespace chromeos
