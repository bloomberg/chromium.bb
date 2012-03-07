// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/stringprintf.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_url_fetcher_factory.h"
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

const char kUberAuthTokenURLFormat[] =
    "%s?source=%s&issueuberauth=1";

}  // namespace


class SigninManagerTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    TokenServiceTestHarness::SetUp();
    manager_.reset(new SigninManager());
    google_login_success_.ListenFor(
        chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
        content::Source<Profile>(profile_.get()));
    google_login_failure_.ListenFor(chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                                    content::Source<Profile>(profile_.get()));
  }

  void SimulateValidResponseSignInWithCredentials() {
    // Simulate the correct StartOAuthLoginTokenFetch response.  This involves
    // two separate fetches.
    TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());

    fetcher->set_url(
      GURL(GaiaUrls::GetInstance()->client_login_to_oauth2_url()));
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(200);
    fetcher->SetResponseString(kGetTokenPairValidResponse);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());

    fetcher->set_url(
        GURL(GaiaUrls::GetInstance()->oauth2_token_url()));
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(200);
    fetcher->SetResponseString(kGetTokenPairValidResponse);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    // Simulate the correct StartUberAuthTokenFetch response.
    GURL url(base::StringPrintf(
        kUberAuthTokenURLFormat,
        GaiaUrls::GetInstance()->oauth1_login_url().c_str(),
        GaiaConstants::kChromeSource));
    fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());
    fetcher->set_url(url);
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(200);
    fetcher->SetResponseString("token");
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    // Simulate the correct StartTokenAuth response.
    fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());

    fetcher->set_url(
        GURL(GaiaUrls::GetInstance()->token_auth_url()));
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(200);
    net::ResponseCookies cookies;
    cookies.push_back("SID=sid");
    cookies.push_back("LSID=lsid");
    fetcher->set_cookies(cookies);
    fetcher->SetResponseString("data");
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    // Simulate the correct GetUserInfo response.
    fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());
    fetcher->set_url(GURL(GaiaUrls::GetInstance()->get_user_info_url()));
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(200);
    fetcher->SetResponseString("email=user@gmail.com\nallServices=");
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SimulateValidResponseClientLogin(bool isGPlusUser) {
    // Simulate the correct ClientLogin response.
    std::string respons_string = isGPlusUser ?
        "email=user@gmail.com\nallServices=googleme" :
        "email=user@gmail.com\nallServices=";
    TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());

    fetcher->set_url(GURL(GaiaUrls::GetInstance()->client_login_url()));
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(200);
    fetcher->SetResponseString("SID=sid\nLSID=lsid\nAuth=auth");
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    // Then simulate the correct GetUserInfo response for the canonical email.
    // A new URL fetcher is used for each call.
    fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());
    fetcher->set_url(GURL(GaiaUrls::GetInstance()->get_user_info_url()));
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(200);
    fetcher->SetResponseString(respons_string);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  TestURLFetcherFactory factory_;
  scoped_ptr<SigninManager> manager_;
  TestNotificationTracker google_login_success_;
  TestNotificationTracker google_login_failure_;
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
