// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/browser_sync/common/browser_sync_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using app_modal::AppModalDialog;
using app_modal::JavaScriptAppModalDialog;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace chromeos {

namespace {

// Email of owner account for test.
const char kTestGaiaId[] = "12345";
const char kTestEmail[] = "username@gmail.com";
const char kTestRawEmail[] = "User.Name@gmail.com";
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

std::string PickAccountId(Profile* profile,
                          const std::string& gaia_id,
                          const std::string& email) {
  return AccountTrackerService::PickAccountIdForAccount(profile->GetPrefs(),
                                                        gaia_id, email);
}

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
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) override {
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

}  // namespace

class OAuth2Test : public OobeBaseTest {
 protected:
  OAuth2Test() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    // Disable sync sinc we don't really need this for these tests and it also
    // makes OAuth2Test.MergeSession test flaky http://crbug.com/408867.
    command_line->AppendSwitch(switches::kDisableSync);
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
    fake_gaia_->SetMergeSessionParams(params);
    SetupGaiaServerWithAccessTokens();
  }

  void SetupGaiaServerForUnexpiredAccount() {
    FakeGaia::MergeSessionParams params;
    params.email = kTestEmail;
    fake_gaia_->SetMergeSessionParams(params);
    SetupGaiaServerWithAccessTokens();
  }

  void SetupGaiaServerForExpiredAccount() {
    FakeGaia::MergeSessionParams params;
    params.gaia_uber_token = kTestGaiaUberToken;
    params.session_sid_cookie = kTestSession2SIDCookie;
    params.session_lsid_cookie = kTestSession2LSIDCookie;
    fake_gaia_->SetMergeSessionParams(params);
    SetupGaiaServerWithAccessTokens();
  }

  void LoginAsExistingUser() {
    content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();

    JsExpect("!!document.querySelector('#account-picker')");
    JsExpect("!!document.querySelector('#pod-row')");

    std::string account_id = PickAccountId(
        ProfileManager::GetPrimaryUserProfile(), kTestGaiaId, kTestEmail);

    EXPECT_EQ(GetOAuthStatusFromLocalState(account_id),
              user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

    // Try login.  Primary profile has changed.
    EXPECT_TRUE(
        TryToLogin(AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId),
                   kTestAccountPassword));
    Profile* profile = ProfileManager::GetPrimaryUserProfile();

    // Wait for the session merge to finish.
    WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

    // Check for existance of refresh token.
    ProfileOAuth2TokenService* token_service =
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
    EXPECT_TRUE(token_service->RefreshTokenIsAvailable(account_id));

    EXPECT_EQ(GetOAuthStatusFromLocalState(account_id),
              user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  }

  bool TryToLogin(const AccountId& account_id, const std::string& password) {
    if (!AddUserToSession(account_id, password))
      return false;

    if (const user_manager::User* active_user =
            user_manager::UserManager::Get()->GetActiveUser()) {
      return active_user->GetAccountId() == account_id;
    }

    return false;
  }

  user_manager::User::OAuthTokenStatus GetOAuthStatusFromLocalState(
      const std::string& account_id) const {
    PrefService* local_state = g_browser_process->local_state();
    const base::DictionaryValue* prefs_oauth_status =
        local_state->GetDictionary("OAuthTokenStatus");
    int oauth_token_status = user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN;
    if (prefs_oauth_status &&
        prefs_oauth_status->GetIntegerWithoutPathExpansion(
            account_id, &oauth_token_status)) {
      user_manager::User::OAuthTokenStatus result =
          static_cast<user_manager::User::OAuthTokenStatus>(oauth_token_status);
      return result;
    }
    return user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN;
  }

 protected:
  // OobeBaseTest overrides.
  Profile* profile() override {
    if (user_manager::UserManager::Get()->GetActiveUser())
      return ProfileManager::GetPrimaryUserProfile();

    return OobeBaseTest::profile();
  }

  bool AddUserToSession(const AccountId& account_id,
                        const std::string& password) {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (!controller) {
      ADD_FAILURE();
      return false;
    }

    UserContext user_context(account_id);
    user_context.SetGaiaID(account_id.GetGaiaId());
    user_context.SetKey(Key(password));
    controller->Login(user_context, SigninSpecifics());
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources()).Wait();
    const user_manager::UserList& logged_users =
        user_manager::UserManager::Get()->GetLoggedInUsers();
    for (user_manager::UserList::const_iterator it = logged_users.begin();
         it != logged_users.end();
         ++it) {
      if ((*it)->GetAccountId() == account_id)
        return true;
    }
    return false;
  }

  void SetupGaiaServerWithAccessTokens() {
    fake_gaia_->MapEmailToGaiaId(kTestEmail, kTestGaiaId);

    // Configure OAuth authentication.
    GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

    // This token satisfies the userinfo.email request from
    // DeviceOAuth2TokenService used in token validation.
    FakeGaia::AccessTokenInfo userinfo_token_info;
    userinfo_token_info.token = kTestUserinfoToken;
    userinfo_token_info.scopes.insert(
        "https://www.googleapis.com/auth/userinfo.email");
    userinfo_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    userinfo_token_info.email = kTestEmail;
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, userinfo_token_info);

    FakeGaia::AccessTokenInfo userinfo_profile_token_info;
    userinfo_profile_token_info.token = kTestUserinfoToken;
    userinfo_profile_token_info.scopes.insert(
        "https://www.googleapis.com/auth/userinfo.profile");
    userinfo_profile_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    userinfo_profile_token_info.email = kTestEmail;
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, userinfo_profile_token_info);

    // The any-api access token for accessing the token minting endpoint.
    FakeGaia::AccessTokenInfo login_token_info;
    login_token_info.token = kTestLoginToken;
    login_token_info.scopes.insert(GaiaConstants::kAnyApiOAuth2Scope);
    login_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, login_token_info);

    // The /auth/chromesync access token for accessing sync endpoint.
    FakeGaia::AccessTokenInfo sync_token_info;
    sync_token_info.token = kTestSyncToken;
    sync_token_info.scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
    sync_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, sync_token_info);

    FakeGaia::AccessTokenInfo auth_login_token_info;
    auth_login_token_info.token = kTestAuthLoginToken;
    auth_login_token_info.scopes.insert(GaiaConstants::kOAuth1LoginScope);
    auth_login_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, auth_login_token_info);
  }

  void CheckSessionState(OAuth2LoginManager::SessionRestoreState state) {
    OAuth2LoginManager* login_manager =
         OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
             profile());
    ASSERT_EQ(state, login_manager->state());
  }

  void WaitForMergeSessionCompletion(
      OAuth2LoginManager::SessionRestoreState final_state) {
    // Wait for the session merge to finish.
    std::set<OAuth2LoginManager::SessionRestoreState> states;
    states.insert(OAuth2LoginManager::SESSION_RESTORE_DONE);
    states.insert(OAuth2LoginManager::SESSION_RESTORE_FAILED);
    states.insert(OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED);
    OAuth2LoginManagerStateWaiter merge_session_waiter(profile());
    merge_session_waiter.WaitForStates(states);
    EXPECT_EQ(merge_session_waiter.final_state(), final_state);
  }

  void StartNewUserSession(bool wait_for_merge) {
    SetupGaiaServerForNewAccount();
    SimulateNetworkOnline();
    WaitForGaiaPageLoad();

    content::WindowedNotificationObserver session_start_waiter(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources());

    // Use capitalized and dotted user name on purpose to make sure
    // our email normalization kicks in.
    GetLoginDisplay()->ShowSigninScreenForCreds(kTestRawEmail,
                                                kTestAccountPassword);
    session_start_waiter.Wait();

    if (wait_for_merge) {
      // Wait for the session merge to finish.
      WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);
    }
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
    context_->GetURLRequestContext()->cookie_store()->GetAllCookiesAsync(
        base::Bind(&CookieReader::OnGetAllCookiesOnUIThread, this));
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

// PRE_MergeSession is testing merge session for a new profile.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_PRE_PRE_MergeSession) {
  StartNewUserSession(true);
  // Check for existance of refresh token.
  std::string account_id = PickAccountId(profile(), kTestGaiaId, kTestEmail);
  ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(
            profile());
  EXPECT_TRUE(token_service->RefreshTokenIsAvailable(account_id));

  EXPECT_EQ(GetOAuthStatusFromLocalState(account_id),
            user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  scoped_refptr<CookieReader> cookie_reader(new CookieReader());
  cookie_reader->ReadCookies(profile());
  EXPECT_EQ(cookie_reader->GetCookieValue("SID"), kTestSessionSIDCookie);
  EXPECT_EQ(cookie_reader->GetCookieValue("LSID"), kTestSessionLSIDCookie);
}

// MergeSession test is running merge session process for an existing profile
// that was generated in PRE_PRE_PRE_MergeSession test. In this test, we
// are not running /MergeSession process since the /ListAccounts call confirms
// that the session is not stale.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_PRE_MergeSession) {
  SetupGaiaServerForUnexpiredAccount();
  SimulateNetworkOnline();
  LoginAsExistingUser();
  scoped_refptr<CookieReader> cookie_reader(new CookieReader());
  cookie_reader->ReadCookies(profile());
  // These are still cookie values form the initial session since
  // /ListAccounts
  EXPECT_EQ(cookie_reader->GetCookieValue("SID"), kTestSessionSIDCookie);
  EXPECT_EQ(cookie_reader->GetCookieValue("LSID"), kTestSessionLSIDCookie);
}

// MergeSession test is running merge session process for an existing profile
// that was generated in PRE_PRE_MergeSession test.
// Disabled due to flakiness: crbug.com/496832
IN_PROC_BROWSER_TEST_F(OAuth2Test, DISABLED_PRE_MergeSession) {
  SetupGaiaServerForExpiredAccount();
  SimulateNetworkOnline();
  LoginAsExistingUser();
  scoped_refptr<CookieReader> cookie_reader(new CookieReader());
  cookie_reader->ReadCookies(profile());
  // These should be cookie values that we generated by calling /MergeSession,
  // since /ListAccounts should have tell us that the initial session cookies
  // are stale.
  EXPECT_EQ(cookie_reader->GetCookieValue("SID"), kTestSession2SIDCookie);
  EXPECT_EQ(cookie_reader->GetCookieValue("LSID"), kTestSession2LSIDCookie);
}

// MergeSession test is attempting to merge session for an existing profile
// that was generated in PRE_PRE_MergeSession test. This attempt should fail
// since FakeGaia instance isn't configured to return relevant tokens/cookies.
// Disabled due to flakiness: crbug.com/496832
IN_PROC_BROWSER_TEST_F(OAuth2Test, DISABLED_MergeSession) {
  SimulateNetworkOnline();

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
    content::NotificationService::AllSources()).Wait();

  JsExpect("!!document.querySelector('#account-picker')");
  JsExpect("!!document.querySelector('#pod-row')");

  std::string account_id = PickAccountId(profile(), kTestGaiaId, kTestEmail);
  EXPECT_EQ(GetOAuthStatusFromLocalState(account_id),
            user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

  EXPECT_TRUE(
      TryToLogin(AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId),
                 kTestAccountPassword));

  // Wait for the session merge to finish.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_FAILED);

  EXPECT_EQ(GetOAuthStatusFromLocalState(account_id),
            user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
}


const char kGooglePageContent[] =
    "<html><title>Hello!</title><script>alert('hello');</script>"
    "<body>Hello Google!</body></html>";
const char kRandomPageContent[] =
    "<html><title>SomthingElse</title><body>I am SomethingElse</body></html>";
const char kHelloPagePath[] = "/hello_google";
const char kRandomPagePath[] = "/non_google_page";


// FakeGoogle serves content of http://www.google.com/hello_google page for
// merge session tests.
class FakeGoogle {
 public:
  FakeGoogle() : start_event_(true, false) {
  }

  ~FakeGoogle() {}

  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // The scheme and host of the URL is actually not important but required to
    // get a valid GURL in order to parse |request.relative_url|.
    GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
    LOG(WARNING) << "Requesting page " << request.relative_url;
    std::string request_path = request_url.path();
    scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
    if (request_path == kHelloPagePath) {  // Serving "google" page.
      start_event_.Signal();
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&FakeGoogle::QuitRunnerOnUIThread,
                     base::Unretained(this)));

      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("text/html");
      http_response->set_content(kGooglePageContent);
    } else if (request_path == kRandomPagePath) {  // Serving "non-google" page.
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("text/html");
      http_response->set_content(kRandomPageContent);
    } else {
      return scoped_ptr<HttpResponse>();      // Request not understood.
    }

    return std::move(http_response);
  }

  // True if we have already served the test page.
  bool IsPageRequested() { return start_event_.IsSignaled(); }

  // Waits until we receive a request to serve the test page.
  void WaitForPageRequest() {
    // If we have already served the request, bail out.
    if (start_event_.IsSignaled())
      return;

    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

 private:
  void QuitRunnerOnUIThread() {
    if (runner_.get())
      runner_->Quit();
  }
  // This event will tell us when we actually see HTTP request on the server
  // side. It should be signalled only after the page/XHR throttle had been
  // removed (after merge session completes).
  base::WaitableEvent start_event_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(FakeGoogle);
};

// FakeGaia specialization that can delay /MergeSession handler until
// we explicitly call DelayedFakeGaia::UnblockMergeSession().
class DelayedFakeGaia : public FakeGaia {
 public:
  DelayedFakeGaia()
     : blocking_event_(true, false),
       start_event_(true, false) {
  }

  void UnblockMergeSession() {
    blocking_event_.Signal();
  }

  void WaitForMergeSessionToStart() {
    // If we have already served the request, bail out.
    if (start_event_.IsSignaled())
      return;

    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

 private:
  // FakeGaia overrides.
  void HandleMergeSession(const HttpRequest& request,
                          BasicHttpResponse* http_response) override {
    start_event_.Signal();
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DelayedFakeGaia::QuitRunnerOnUIThread,
                   base::Unretained(this)));
    blocking_event_.Wait();
    FakeGaia::HandleMergeSession(request, http_response);
  }

  void QuitRunnerOnUIThread() {
    if (runner_.get())
      runner_->Quit();
  }

  base::WaitableEvent blocking_event_;
  base::WaitableEvent start_event_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(DelayedFakeGaia);
};

class MergeSessionTest : public OAuth2Test {
 protected:
  MergeSessionTest() : delayed_fake_gaia_(new DelayedFakeGaia()) {
    fake_gaia_.reset(delayed_fake_gaia_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OAuth2Test::SetUpCommandLine(command_line);

    // Get fake URL for fake google.com.
    const GURL& server_url = embedded_test_server()->base_url();
    GURL::Replacements replace_google_host;
    replace_google_host.SetHostStr("www.google.com");
    GURL google_url = server_url.ReplaceComponents(replace_google_host);
    fake_google_page_url_ = google_url.Resolve(kHelloPagePath);

    GURL::Replacements replace_non_google_host;
    replace_non_google_host.SetHostStr("www.somethingelse.org");
    GURL non_google_url = server_url.ReplaceComponents(replace_non_google_host);
    non_google_page_url_ = non_google_url.Resolve(kRandomPagePath);
}

void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&FakeGoogle::HandleRequest,
                   base::Unretained(&fake_google_)));
    OAuth2Test::SetUp();
  }

 protected:
  void UnblockMergeSession() {
    delayed_fake_gaia_->UnblockMergeSession();
  }

  void WaitForMergeSessionToStart() {
    delayed_fake_gaia_->WaitForMergeSessionToStart();
  }

  void JsExpect(content::WebContents* contents,
                const std::string& expression) {
    bool result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "window.domAutomationController.send(!!(" + expression + "));",
         &result));
    ASSERT_TRUE(result) << expression;
  }

  const GURL& GetBackGroundPageUrl(const std::string& extension_id) {
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(profile());
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    return host->host_contents()->GetURL();
  }

  void JsExpectOnBackgroundPage(const std::string& extension_id,
                                const std::string& expression) {
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(profile());
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    if (host == NULL) {
      ADD_FAILURE() << "Extension " << extension_id
                    << " has no background page.";
      return;
    }

    JsExpect(host->host_contents(), expression);
  }

  FakeGoogle fake_google_;
  DelayedFakeGaia* delayed_fake_gaia_;
  GURL fake_google_page_url_;
  GURL non_google_page_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MergeSessionTest);
};

Browser* FindOrCreateVisibleBrowser(Profile* profile) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  Browser* browser = displayer.browser();
  if (browser->tab_strip_model()->count() == 0)
    chrome::AddTabAt(browser, GURL(), -1, true);
  return browser;
}

IN_PROC_BROWSER_TEST_F(MergeSessionTest, PageThrottle) {
  StartNewUserSession(false);

  // Try to open a page from google.com.
  Browser* browser =
      FindOrCreateVisibleBrowser(profile());
  ui_test_utils::NavigateToURLWithDisposition(
      browser,
      fake_google_page_url_,
      CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  // Wait until we get send merge session request.
  WaitForMergeSessionToStart();

  // Make sure the page is blocked by the throttle.
  EXPECT_FALSE(fake_google_.IsPageRequested());

  // Check that throttle page is displayed instead.
  base::string16 title;
  ui_test_utils::GetCurrentTabTitle(browser, &title);
  DVLOG(1) << "Loaded page at the start : " << title;

  // Unblock GAIA request.
  UnblockMergeSession();

  // Wait for the session merge to finish.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Make sure the test page is served.
  fake_google_.WaitForPageRequest();

  // Check that real page is no longer blocked by the throttle and that the
  // real page pops up JS dialog.
  AppModalDialog* dialog = ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(dialog->IsJavaScriptModalDialog());
  JavaScriptAppModalDialog* js_dialog =
      static_cast<JavaScriptAppModalDialog*>(dialog);
  js_dialog->native_dialog()->AcceptAppModalDialog();

  ui_test_utils::GetCurrentTabTitle(browser, &title);
  DVLOG(1) << "Loaded page at the end : " << title;
}

IN_PROC_BROWSER_TEST_F(MergeSessionTest, XHRThrottle) {
  StartNewUserSession(false);

  // Wait until we get send merge session request.
  WaitForMergeSessionToStart();

  // Reset ExtensionBrowserTest::observer_ to the right browser object.
  Browser* browser = FindOrCreateVisibleBrowser(profile());
  observer_.reset(new ExtensionTestNotificationObserver(browser));

  // Run background page tests. The tests will just wait for XHR request
  // to complete.
  extensions::ResultCatcher catcher;

  scoped_ptr<ExtensionTestMessageListener> non_google_xhr_listener(
      new ExtensionTestMessageListener("non-google-xhr-received", false));

  // Load extension with a background page. The background page will
  // attempt to load |fake_google_page_url_| via XHR.
  const extensions::Extension* ext = LoadExtension(
      test_data_dir_.AppendASCII("merge_session"));
  ASSERT_TRUE(ext);

  // Kick off XHR request from the extension.
  JsExpectOnBackgroundPage(
      ext->id(),
      base::StringPrintf("startThrottledTests('%s', '%s')",
                         fake_google_page_url_.spec().c_str(),
                         non_google_page_url_.spec().c_str()));

  // Verify that we've sent XHR request form the extension side...
  JsExpectOnBackgroundPage(ext->id(),
                           "googleRequestSent && !googleResponseReceived");

  // ...but didn't see it on the server side yet.
  EXPECT_FALSE(fake_google_.IsPageRequested());

  // Unblock GAIA request.
  UnblockMergeSession();

  // Wait for the session merge to finish.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Wait until non-google XHR content to load first.
  ASSERT_TRUE(non_google_xhr_listener->WaitUntilSatisfied());

  if (!catcher.GetNextResult()) {
    std::string message = catcher.message();
    ADD_FAILURE() << "Tests failed: " << message;
  }

  EXPECT_TRUE(fake_google_.IsPageRequested());
}

}  // namespace chromeos
