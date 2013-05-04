// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/chrome_signin_manager_delegate.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webdata/encryptor/encryptor.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

const char kGetTokenPairValidResponse[] =
    "{"
    "  \"refresh_token\": \"rt1\","
    "  \"access_token\": \"at1\","
    "  \"expires_in\": 3600,"
    "  \"token_type\": \"Bearer\""
    "}";

const char kUberAuthTokenURLFormat[] = "%s?source=%s&issueuberauth=1";

}  // namespace


class SigninManagerTest : public TokenServiceTestHarness {
 public:
  void SetUpOnIOThread(base::WaitableEvent* io_setup_complete) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // This is a workaround for a bug in the TestingProfile.
    // The URLRequestContext will be created by GetCookieMonster on the UI
    // thread, if it does not already exist. But it must be created on the IO
    // thread or else it will DCHECK upon destruction.
    // Force it to be created here.
    profile_->CreateRequestContext();
    profile_->GetRequestContext()->GetURLRequestContext();
    io_setup_complete->Signal();
  }

  virtual void SetUp() OVERRIDE {
    prefs_.reset(new TestingPrefServiceSimple);
    chrome::RegisterLocalState(prefs_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(
        prefs_.get());
    TokenServiceTestHarness::SetUp();
    manager_.reset(new SigninManager(
        scoped_ptr<SigninManagerDelegate>(
            new ChromeSigninManagerDelegate(profile_.get()))));
    google_login_success_.ListenFor(
        chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
        content::Source<Profile>(profile_.get()));
    google_login_failure_.ListenFor(chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                                    content::Source<Profile>(profile_.get()));

    io_thread_.reset(new content::TestBrowserThread(BrowserThread::IO));
    ASSERT_TRUE(io_thread_->Start());
    base::WaitableEvent io_setup_complete(true, false);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SigninManagerTest::SetUpOnIOThread,
                   base::Unretained(this), &io_setup_complete));
    io_setup_complete.Wait();
  }

  virtual void TearDown() OVERRIDE {
    // Destroy the SigninManager here, because it relies on profile_ which is
    // freed in the base class.
    manager_->Shutdown();
    manager_.reset(NULL);
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
    prefs_.reset(NULL);
    TokenServiceTestHarness::TearDown();
    io_thread_->Stop();
  }

  void SetupFetcherAndComplete(const std::string& url,
                               int response_code,
                               const net::ResponseCookies& cookies,
                               const std::string& response_string) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());

    cookies_.insert(cookies_.end(), cookies.begin(), cookies.end());
    fetcher->set_url(GURL(url));
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_string);
    fetcher->set_cookies(cookies);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SimulateValidResponseSignInWithCredentials() {
    // Simulate the correct StartOAuthLoginTokenFetch response.  This involves
    // two separate fetches.
    SetupFetcherAndComplete(
        GaiaUrls::GetInstance()->client_login_to_oauth2_url(), 200,
        net::ResponseCookies(), kGetTokenPairValidResponse);

    SetupFetcherAndComplete(GaiaUrls::GetInstance()->oauth2_token_url(), 200,
                            net::ResponseCookies(), kGetTokenPairValidResponse);

    // Simulate the correct StartOAuthLogin response.
    SetupFetcherAndComplete(GaiaUrls::GetInstance()->oauth1_login_url(), 200,
                            net::ResponseCookies(),
                            "SID=sid\nLSID=lsid\nAuth=auth_token");

    SimulateValidResponseGetClientInfo(false);
  }

  void SimulateValidResponseClientLogin(bool isGPlusUser) {
    SetupFetcherAndComplete(GaiaUrls::GetInstance()->client_login_url(), 200,
                            net::ResponseCookies(),
                            "SID=sid\nLSID=lsid\nAuth=auth");
    SimulateValidResponseGetClientInfo(isGPlusUser);
  }

  void SimulateValidResponseGetClientInfo(bool isGPlusUser) {
    // Simulate the correct ClientLogin response.
    std::string response_string = isGPlusUser ?
        "email=user@gmail.com\ndisplayEmail=USER@gmail.com\n"
        "allServices=googleme" :
        "email=user@gmail.com\ndisplayEmail=USER@gmail.com\n"
        "allServices=";
    SetupFetcherAndComplete(GaiaUrls::GetInstance()->get_user_info_url(), 200,
                            net::ResponseCookies(), response_string);
  }

  void SimulateValidUberToken() {
    SetupFetcherAndComplete(GaiaUrls::GetInstance()->oauth2_token_url(), 200,
                            net::ResponseCookies(), kGetTokenPairValidResponse);
    std::string  uberauth_token_gurl = base::StringPrintf(
        kUberAuthTokenURLFormat,
        GaiaUrls::GetInstance()->oauth1_login_url().c_str(),
        "source");
    SetupFetcherAndComplete(uberauth_token_gurl, 200,
                            net::ResponseCookies(), "ut1");

    net::ResponseCookies cookies;
    cookies.push_back("checkCookie = true");
    SetupFetcherAndComplete(GaiaUrls::GetInstance()->merge_session_url(), 200,
                            cookies, "<html></html>");
  }

  void WaitUntilUIDone() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SigninManagerTest::NotifyUIOnComplete,
                   base::Unretained(this)));
    MessageLoop::current()->Run();
  }

  void NotifyUIOnComplete () {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      // Redo on UI thread.
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&SigninManagerTest::NotifyUIOnComplete,
                     base::Unretained(this)));
      return;
    }
    MessageLoop::current()->Quit();
  }

  void ExpectSignInWithCredentialsSuccess() {
    EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

    SimulateValidResponseSignInWithCredentials();

    EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());

    // This is flow, the oauth2 credentials should already be available in
    // the token service.
    EXPECT_TRUE(service_->HasOAuthLoginToken());

    // Should go into token service and stop.
    EXPECT_EQ(1U, google_login_success_.size());
    EXPECT_EQ(0U, google_login_failure_.size());

    // Should persist across resets.
    manager_->Shutdown();
    manager_.reset(new SigninManager(
        scoped_ptr<SigninManagerDelegate>(
            new ChromeSigninManagerDelegate(profile_.get()))));
    manager_->Initialize(profile_.get());
    EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());
  }

  // Helper method that wraps the logic when signin with credentials
  // should fail. If |requestSent| is true, then simulate valid resopnse.
  // Otherwise the sign-in is aborted before any request is sent, thus no need
  // to simulatate response.
  void ExpectSignInWithCredentialsFail(bool requestSent) {
    EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

    if (requestSent)
      SimulateValidResponseSignInWithCredentials();

    // The oauth2 credentials should not be available in the token service
    // because the email was incorrect.
    EXPECT_FALSE(service_->HasOAuthLoginToken());

    // Should go into token service and stop.
    EXPECT_EQ(0U, google_login_success_.size());
    EXPECT_EQ(1U, google_login_failure_.size());
  }

  void CompleteSigninCallback(const std::string& oauth_token) {
    oauth_tokens_fetched_.push_back(oauth_token);
    manager_->CompletePendingSignin();
  }

  void CancelSigninCallback(const std::string& oauth_token) {
    oauth_tokens_fetched_.push_back(oauth_token);
    manager_->SignOut();
  }

  net::TestURLFetcherFactory factory_;
  scoped_ptr<SigninManager> manager_;
  content::TestNotificationTracker google_login_success_;
  content::TestNotificationTracker google_login_failure_;
  std::vector<std::string> oauth_tokens_fetched_;
  scoped_ptr<TestingPrefServiceSimple> prefs_;
  scoped_ptr<content::TestBrowserThread> io_thread_;
  std::vector<std::string> cookies_;
};

// NOTE: ClientLogin's "StartSignin" is called after collecting credentials
//       from the user.
TEST_F(SigninManagerTest, SignInClientLogin) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignIn(
      "user@gmail.com", "password", std::string(), std::string());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  SimulateValidResponseClientLogin(true);
  EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());

  // Should go into token service and stop.
  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  service_->OnIssueAuthTokenSuccess(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                    "oauth2Token");
  SimulateValidUberToken();
  // Check that the login cookie has been sent.
  ASSERT_NE(std::find(cookies_.begin(), cookies_.end(), "checkCookie = true"),
            cookies_.end());

  // Should persist across resets.
  manager_->Shutdown();
  manager_.reset(new SigninManager(
      scoped_ptr<SigninManagerDelegate>(
          new ChromeSigninManagerDelegate(profile_.get()))));
  manager_->Initialize(profile_.get());
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, SignInWithCredentials) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignInWithCredentials(
      "0",
      "user@gmail.com",
      "password",
      SigninManager::OAuthTokenFetchedCallback());

  ExpectSignInWithCredentialsSuccess();
}

TEST_F(SigninManagerTest, SignInWithCredentialsNonCanonicalEmail) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignInWithCredentials(
      "0",
      "user",
      "password",
      SigninManager::OAuthTokenFetchedCallback());

  ExpectSignInWithCredentialsSuccess();
}

TEST_F(SigninManagerTest, SignInWithCredentialsWrongEmail) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  // If the email address used to start the sign in does not match the
  // email address returned by /GetUserInfo, the sign in should fail.
  manager_->StartSignInWithCredentials(
      "0",
      "user2@gmail.com",
      "password",
      SigninManager::OAuthTokenFetchedCallback());

  ExpectSignInWithCredentialsFail(true /* requestSent */);
}

TEST_F(SigninManagerTest, SignInWithCredentialsEmptyPasswordValidCookie) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  // Set a valid LSID cookie in the test cookie store.
  scoped_refptr<net::CookieMonster> cookie_monster =
      profile_->GetCookieMonster();
  net::CookieOptions options;
  options.set_include_httponly();
  cookie_monster->SetCookieWithOptionsAsync(
        GURL("https://accounts.google.com"),
        "LSID=1234; secure; httponly", options,
        net::CookieMonster::SetCookiesCallback());

  // Since the password is empty, will verify the gaia cookies first.
  manager_->StartSignInWithCredentials(
      "0",
      "user@gmail.com",
      std::string(),
      SigninManager::OAuthTokenFetchedCallback());

  WaitUntilUIDone();

  // Verification should succeed and continue with auto signin.
  ExpectSignInWithCredentialsSuccess();
}

TEST_F(SigninManagerTest, SignInWithCredentialsEmptyPasswordNoValidCookie) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  // Since the password is empty, will verify the gaia cookies first.
  manager_->StartSignInWithCredentials(
      "0",
      "user@gmail.com",
      std::string(),
      SigninManager::OAuthTokenFetchedCallback());

  WaitUntilUIDone();

  // Since the test cookie store is empty, verification should fail and throws
  // a login error.
  ExpectSignInWithCredentialsFail(false /* requestSent */);
}

TEST_F(SigninManagerTest, SignInWithCredentialsEmptyPasswordInValidCookie) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  // Set an invalid LSID cookie in the test cookie store.
  scoped_refptr<net::CookieMonster> cookie_monster =
      profile_->GetCookieMonster();
  net::CookieOptions options;
  options.set_include_httponly();
  cookie_monster->SetCookieWithOptionsAsync(
        GURL("https://accounts.google.com"),
        "LSID=1234; domain=google.com; secure; httponly", options,
        net::CookieMonster::SetCookiesCallback());

  // Since the password is empty, must verify the gaia cookies first.
  manager_->StartSignInWithCredentials(
      "0",
      "user@gmail.com",
      std::string(),
      SigninManager::OAuthTokenFetchedCallback());

  WaitUntilUIDone();

  // Since the LSID cookie is invalid, verification should fail and throws
  // a login error.
  ExpectSignInWithCredentialsFail(false /* requestSent */);
}

TEST_F(SigninManagerTest, SignInWithCredentialsCallbackComplete) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  // Since the password is empty, must verify the gaia cookies first.
  SigninManager::OAuthTokenFetchedCallback callback =
      base::Bind(&SigninManagerTest::CompleteSigninCallback,
                 base::Unretained(this));
  manager_->StartSignInWithCredentials(
      "0",
      "user@gmail.com",
      "password",
      callback);

  ExpectSignInWithCredentialsSuccess();
  ASSERT_EQ(1U, oauth_tokens_fetched_.size());
  EXPECT_EQ(oauth_tokens_fetched_[0], "rt1");
}

TEST_F(SigninManagerTest, SignInWithCredentialsCallbackCancel) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  // Since the password is empty, must verify the gaia cookies first.
  SigninManager::OAuthTokenFetchedCallback callback =
      base::Bind(&SigninManagerTest::CancelSigninCallback,
                 base::Unretained(this));
  manager_->StartSignInWithCredentials(
      "0",
      "user@gmail.com",
      "password",
      callback);

  // Signin should fail since it would be cancelled by the callback.
  ExpectSignInWithCredentialsFail(true);
  ASSERT_EQ(1U, oauth_tokens_fetched_.size());
  EXPECT_EQ(oauth_tokens_fetched_[0], "rt1");
}

TEST_F(SigninManagerTest, SignInClientLoginNoGPlus) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignIn("username", "password", std::string(), std::string());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  SimulateValidResponseClientLogin(false);
  EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, ClearTransientSigninData) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignIn("username", "password", std::string(), std::string());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  SimulateValidResponseClientLogin(false);

  // Should go into token service and stop.
  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());

  // Now clear the in memory data.
  manager_->ClearTransientSigninData();
  EXPECT_TRUE(manager_->last_result_.data.empty());
  EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());

  // Ensure preferences are not modified.
  EXPECT_FALSE(
     profile_->GetPrefs()->GetString(prefs::kGoogleServicesUsername).empty());

  // On reset it should be regenerated.
  manager_->Shutdown();
  manager_.reset(new SigninManager(
      scoped_ptr<SigninManagerDelegate>(
          new ChromeSigninManagerDelegate(profile_.get()))));
  manager_->Initialize(profile_.get());

  // Now make sure we have the right user name.
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, SignOutClientLogin) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", std::string(), std::string());
  SimulateValidResponseClientLogin(false);
  manager_->OnClientLoginSuccess(credentials_);

  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());
  manager_->SignOut();
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
  // Should not be persisted anymore
  manager_->Shutdown();
  manager_.reset(new SigninManager(
      scoped_ptr<SigninManagerDelegate>(
          new ChromeSigninManagerDelegate(profile_.get()))));
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, SignInFailureClientLogin) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", std::string(), std::string());
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  manager_->OnClientLoginFailure(error);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());

  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  // Should not be persisted
  manager_->Shutdown();
  manager_.reset(new SigninManager(
      scoped_ptr<SigninManagerDelegate>(
          new ChromeSigninManagerDelegate(profile_.get()))));
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, ProvideSecondFactorSuccess) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", std::string(), std::string());
  GoogleServiceAuthError error(GoogleServiceAuthError::TWO_FACTOR);
  manager_->OnClientLoginFailure(error);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());

  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
  EXPECT_FALSE(manager_->possibly_invalid_username_.empty());

  manager_->ProvideSecondFactorAccessCode("access");
  SimulateValidResponseClientLogin(false);

  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());
}

TEST_F(SigninManagerTest, ProvideSecondFactorFailure) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", std::string(), std::string());
  GoogleServiceAuthError error1(GoogleServiceAuthError::TWO_FACTOR);
  manager_->OnClientLoginFailure(error1);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());

  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
  EXPECT_FALSE(manager_->possibly_invalid_username_.empty());

  manager_->ProvideSecondFactorAccessCode("badaccess");
  GoogleServiceAuthError error2(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  manager_->OnClientLoginFailure(error2);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(2U, google_login_failure_.size());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->ProvideSecondFactorAccessCode("badaccess");
  GoogleServiceAuthError error3(GoogleServiceAuthError::CONNECTION_FAILED);
  manager_->OnClientLoginFailure(error3);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(3U, google_login_failure_.size());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, SignOutMidConnect) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", std::string(), std::string());
  EXPECT_EQ("username", manager_->GetUsernameForAuthInProgress());
  manager_->SignOut();
  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());

  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
  EXPECT_TRUE(manager_->GetUsernameForAuthInProgress().empty());
}

TEST_F(SigninManagerTest, SignOutWhileProhibited) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->SetAuthenticatedUsername("user@gmail.com");
  manager_->ProhibitSignout(true);
  manager_->SignOut();
  EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());
  manager_->ProhibitSignout(false);
  manager_->SignOut();
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, TestIsWebBasedSigninFlowURL) {
  EXPECT_FALSE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("http://www.google.com")));
  EXPECT_TRUE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("https://accounts.google.com/ServiceLogin?service=chromiumsync")));
  EXPECT_FALSE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("http://accounts.google.com/ServiceLogin?service=chromiumsync")));
  // http, not https, should not be treated as web based signin.
  EXPECT_FALSE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("http://accounts.google.com/ServiceLogin?service=googlemail")));
  // chromiumsync is double-embedded in a continue query param.
  EXPECT_TRUE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("https://accounts.google.com/CheckCookie?"
           "continue=https%3A%2F%2Fwww.google.com%2Fintl%2Fen-US%2Fchrome"
           "%2Fblank.html%3Fsource%3D3%26nonadv%3D1&service=chromiumsync")));
}
