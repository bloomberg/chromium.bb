// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGetTokenPairValidResponse[] =
    "{"
    "  \"refresh_token\": \"rt1\","
    "  \"access_token\": \"at1\","
    "  \"expires_in\": 3600,"
    "  \"token_type\": \"Bearer\""
    "}";

}  // namespace


class SigninManagerTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    prefs_.reset(new TestingPrefService);
    chrome::RegisterLocalState(prefs_.get());
    static_cast<TestingBrowserProcess*>(g_browser_process)->SetLocalState(
        prefs_.get());
    TokenServiceTestHarness::SetUp();
    manager_.reset(new SigninManager());
    google_login_success_.ListenFor(
        chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
        content::Source<Profile>(profile_.get()));
    google_login_failure_.ListenFor(chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                                    content::Source<Profile>(profile_.get()));
  }

  virtual void TearDown() OVERRIDE {
    // Destroy the SigninManager here, because it relies on profile_ which is
    // freed in the base class.
    manager_.reset(NULL);
    static_cast<TestingBrowserProcess*>(g_browser_process)->SetLocalState(NULL);
    prefs_.reset(NULL);
    TokenServiceTestHarness::TearDown();
  }

  void SetupFetcherAndComplete(const std::string& url,
                               int response_code,
                               const net::ResponseCookies& cookies,
                               const std::string& response_string) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());

    fetcher->set_url(GURL(url));
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_string);
    fetcher->set_cookies(cookies);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SimulateValidResponseSignInWithOAuth() {
    // Simulate the correct StartClientOAuth response.  This involves
    // separate fetches.
    SetupFetcherAndComplete(GaiaUrls::GetInstance()->client_oauth_url(), 200,
                            net::ResponseCookies(),
                            "{"
                            "  \"oauth2\": {"
                            "    \"refresh_token\": \"rt1\","
                            "    \"access_token\": \"at1\","
                            "    \"expires_in\": 3600,"
                            "    \"token_type\": \"Bearer\""
                            "  }"
                            "}");

    SetupFetcherAndComplete(GaiaUrls::GetInstance()->oauth1_login_url(), 200,
                            net::ResponseCookies(),
                            "SID=sid\nLSID=lsid\nAuth=auth_token");

    SimulateValidResponseGetClientInfo(false);
  }

  void SimulateSignInWithOAuthChallengeCaptcha() {
    SetupFetcherAndComplete(GaiaUrls::GetInstance()->oauth1_login_scope(), 200,
                            net::ResponseCookies(),
                            "{"
                            "  \"cause\" : \"NeedsAdditional\","
                            "  \"fallback\" : {"
                            "    \"name\" : \"Terminating\","
                            "    \"url\" : \"https://www.terminating.com\""
                            "  },"
                            "  \"challenge\" : {"
                            "    \"name\" : \"Captcha\","
                            "    \"image_url\" : \"http://www.image.com/\","
                            "    \"image_width\" : 640,"
                            "    \"image_height\" : 480,"
                            "    \"audio_url\" : \"http://www.audio.com/\","
                            "    \"challenge_token\" : \"challengetokenblob\""
                            "  }"
                            "}");
  }

  void SimulateSignInWithOAuthChallengeOtp() {
    SetupFetcherAndComplete(GaiaUrls::GetInstance()->oauth1_login_scope(), 200,
                            net::ResponseCookies(),
                            "{"
                            "  \"cause\" : \"NeedsAdditional\","
                            "  \"fallback\" : {"
                            "    \"name\" : \"Terminating\","
                            "    \"url\" : \"https://www.terminating.com\""
                            "  },"
                            "  \"challenge\" : {"
                            "    \"name\" : \"TwoStep\","
                            "    \"prompt_text\" : \"prompt_text\","
                            "    \"alternate_text\" : \"alternate_text\","
                            "    \"challenge_token\" : \"challengetokenblob\","
                            "    \"field_length\" : 10"
                            "  }"
                            "}");
  }

  void SimulateProvideOAuthChallengeResponseValid() {
    SimulateValidResponseSignInWithOAuth();
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
        "email=user@gmail.com\nallServices=googleme" :
        "email=user@gmail.com\nallServices=";
    SetupFetcherAndComplete(GaiaUrls::GetInstance()->get_user_info_url(), 200,
                            net::ResponseCookies(), response_string);
  }

  net::TestURLFetcherFactory factory_;
  scoped_ptr<SigninManager> manager_;
  content::TestNotificationTracker google_login_success_;
  content::TestNotificationTracker google_login_failure_;
  scoped_ptr<TestingPrefService> prefs_;
};

// NOTE: ClientLogin's "StartSignin" is called after collecting credentials
//       from the user.
TEST_F(SigninManagerTest, SignInClientLogin) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignIn("user@gmail.com", "password", "", "");
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  SimulateValidResponseClientLogin(true);
  EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kIsGooglePlusUser));

  // Should go into token service and stop.
  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  // Should persist across resets.
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, Prohibited) {
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(manager_->IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername(""));
}

TEST_F(SigninManagerTest, TestAlternateWildcard) {
  // Test to make sure we accept "*@google.com" as a pattern (treat it as if
  // the admin entered ".*@google.com").
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, "*@google.com");
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(manager_->IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername(""));
}

TEST_F(SigninManagerTest, ProhibitedAtStartup) {
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "monkey@invalid.com");
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  manager_->Initialize(profile_.get());
  // Currently signed in user is prohibited by policy, so should be signed out.
  EXPECT_EQ("", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, ProhibitedAfterStartup) {
  std::string user("monkey@invalid.com");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, user);
  manager_->Initialize(profile_.get());
  EXPECT_EQ(user, manager_->GetAuthenticatedUsername());
  // Update the profile - user should be signed out.
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  EXPECT_EQ("", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, SignInWithCredentials) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignInWithCredentials("0", "user@gmail.com", "password");
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  SimulateValidResponseSignInWithCredentials();
  EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());

  // Should go into token service and stop.
  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  // Should persist across resets.
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, SignInClientLoginNoGPlus) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignIn("username", "password", "", "");
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  SimulateValidResponseClientLogin(false);
  EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kIsGooglePlusUser));
}

TEST_F(SigninManagerTest, ClearTransientSigninData) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignIn("username", "password", "", "");
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
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());

  // Now make sure we have the right user name.
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, SignOutClientLogin) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
  SimulateValidResponseClientLogin(false);
  manager_->OnClientLoginSuccess(credentials_);

  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());
  manager_->SignOut();
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kIsGooglePlusUser));
  // Should not be persisted anymore
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, SignInFailureClientLogin) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  manager_->OnClientLoginFailure(error);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());

  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  // Should not be persisted
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, ProvideSecondFactorSuccess) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
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
  manager_->StartSignIn("username", "password", "", "");
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
  manager_->StartSignIn("username", "password", "", "");
  manager_->SignOut();
  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, SignInWithOAuth) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignInWithOAuth("user@gmail.com", "password");
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  SimulateValidResponseSignInWithOAuth();
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());

  // Should go into token service and stop.
  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());
}

TEST_F(SigninManagerTest, SignInWithOAuthChallengeCaptcha) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignInWithOAuth("user@gmail.com", "password");
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  SimulateSignInWithOAuthChallengeCaptcha();

  manager_->ProvideOAuthChallengeResponse(
      GoogleServiceAuthError::CAPTCHA_REQUIRED, "token", "solution");

  SimulateProvideOAuthChallengeResponseValid();
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());

  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());
}

TEST_F(SigninManagerTest, SignInWithOAuthChallengeOtp) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignInWithOAuth("user@gmail.com", "password");
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  SimulateSignInWithOAuthChallengeOtp();

  manager_->ProvideOAuthChallengeResponse(
      GoogleServiceAuthError::CAPTCHA_REQUIRED, "token", "solution");

  SimulateProvideOAuthChallengeResponseValid();
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());

  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());
}
