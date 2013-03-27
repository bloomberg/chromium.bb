// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/wallet/wallet_signin_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stringprintf.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/browser/wallet/wallet_service_url.h"
#include "components/autofill/browser/wallet/wallet_signin_helper_delegate.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::_;

namespace autofill {
namespace wallet {

namespace {

const char kGetTokenPairValidResponse[] =
    "{"
    "  \"refresh_token\": \"rt1\","
    "  \"access_token\": \"at1\","
    "  \"expires_in\": 3600,"
    "  \"token_type\": \"Bearer\""
    "}";

const char kGetAccountInfoValidResponseFormat[] =
    "{"
    "  \"email\": \"%s\""
    "}";

class MockWalletSigninHelperDelegate : public WalletSigninHelperDelegate {
 public:
  MOCK_METHOD1(OnPassiveSigninSuccess, void(const std::string& username));
  MOCK_METHOD1(OnAutomaticSigninSuccess, void(const std::string& username));
  MOCK_METHOD1(OnUserNameFetchSuccess, void(const std::string& username));
  MOCK_METHOD1(OnPassiveSigninFailure,
               void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnAutomaticSigninFailure,
               void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnUserNameFetchFailure,
               void(const GoogleServiceAuthError& error));
};

class WalletSigninHelperForTesting : public WalletSigninHelper {
 public:
  WalletSigninHelperForTesting(WalletSigninHelperDelegate* delegate,
                               net::URLRequestContextGetter* getter)
      : WalletSigninHelper(delegate, getter) {
  }

  // Bring in the test-only getters.
  using WalletSigninHelper::GetGetAccountInfoUrlForTesting;
  using WalletSigninHelper::state;

  // Bring in the State enum.
  using WalletSigninHelper::State;
  using WalletSigninHelper::IDLE;
};

}  // namespace

class WalletSigninHelperTest : public testing::Test {
 public:
  WalletSigninHelperTest() : io_thread_(content::BrowserThread::IO) {}

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();
    profile_.CreateRequestContext();
    signin_helper_.reset(new WalletSigninHelperForTesting(
        &mock_delegate_,
        profile_.GetRequestContext()));
    EXPECT_EQ(WalletSigninHelperForTesting::IDLE, state());
  }

  virtual void TearDown() OVERRIDE {
    signin_helper_.reset();
    profile_.ResetRequestContext();
    io_thread_.Stop();
  }

 protected:
  // Sets up a response for the mock URLFetcher and completes the request.
  void SetUpFetcherResponseAndCompleteRequest(
      const std::string& url,
      int response_code,
      const net::ResponseCookies& cookies,
      const std::string& response_string) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    ASSERT_TRUE(fetcher->delegate());

    fetcher->set_url(GURL(url));
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_string);
    fetcher->set_cookies(cookies);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void MockSuccessfulOAuthLoginResponse() {
    SetUpFetcherResponseAndCompleteRequest(
        GaiaUrls::GetInstance()->client_login_url(), 200,
        net::ResponseCookies(),
        "SID=sid\nLSID=lsid\nAuth=auth");
  }

  void MockFailedOAuthLoginResponse404() {
    SetUpFetcherResponseAndCompleteRequest(
        GaiaUrls::GetInstance()->client_login_url(), 404,
        net::ResponseCookies(),
        "");
  }

  void MockSuccessfulGaiaUserInfoResponse(const std::string& username) {
    SetUpFetcherResponseAndCompleteRequest(
        GaiaUrls::GetInstance()->get_user_info_url(), 200,
        net::ResponseCookies(),
        "email=" + username);
  }

  void MockFailedGaiaUserInfoResponse404() {
    SetUpFetcherResponseAndCompleteRequest(
        GaiaUrls::GetInstance()->get_user_info_url(), 404,
        net::ResponseCookies(),
        "");
  }

  void MockSuccessfulGetAccountInfoResponse(const std::string& username) {
    SetUpFetcherResponseAndCompleteRequest(
        signin_helper_->GetGetAccountInfoUrlForTesting(), 200,
        net::ResponseCookies(),
        base::StringPrintf(
            kGetAccountInfoValidResponseFormat,
            username.c_str()));
  }

  void MockFailedGetAccountInfoResponse404() {
    SetUpFetcherResponseAndCompleteRequest(
        signin_helper_->GetGetAccountInfoUrlForTesting(), 404,
        net::ResponseCookies(),
        "");
  }

  void MockSuccessfulPassiveAuthUrlMergeAndRedirectResponse() {
    SetUpFetcherResponseAndCompleteRequest(
        wallet::GetPassiveAuthUrl().spec(), 200,
        net::ResponseCookies(),
        "");
  }

  void MockFailedPassiveAuthUrlMergeAndRedirectResponse404() {
    SetUpFetcherResponseAndCompleteRequest(
        wallet::GetPassiveAuthUrl().spec(), 404,
        net::ResponseCookies(),
        "");
  }

  WalletSigninHelperForTesting::State state() const {
    return signin_helper_->state();
  }

  scoped_ptr<WalletSigninHelperForTesting> signin_helper_;
  MockWalletSigninHelperDelegate mock_delegate_;

 private:
  // The profile's request context must be released on the IO thread.
  content::TestBrowserThread io_thread_;
  net::TestURLFetcherFactory factory_;
  TestingProfile profile_;
};

TEST_F(WalletSigninHelperTest, PassiveSigninSuccessful) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninSuccess("user@gmail.com"));
  signin_helper_->StartPassiveSignin();
  MockSuccessfulPassiveAuthUrlMergeAndRedirectResponse();
  MockSuccessfulGetAccountInfoResponse("user@gmail.com");
}

TEST_F(WalletSigninHelperTest, PassiveSigninFailedSignin) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninFailure(_));
  signin_helper_->StartPassiveSignin();
  MockFailedPassiveAuthUrlMergeAndRedirectResponse404();
}

TEST_F(WalletSigninHelperTest, PassiveSigninFailedUserInfo) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninFailure(_));
  signin_helper_->StartPassiveSignin();
  MockSuccessfulPassiveAuthUrlMergeAndRedirectResponse();
  MockFailedGetAccountInfoResponse404();
}

TEST_F(WalletSigninHelperTest, PassiveUserInfoSuccessful) {
  EXPECT_CALL(mock_delegate_, OnUserNameFetchSuccess("user@gmail.com"));
  signin_helper_->StartUserNameFetch();
  MockSuccessfulGetAccountInfoResponse("user@gmail.com");
}

TEST_F(WalletSigninHelperTest, PassiveUserInfoFailedUserInfo) {
  EXPECT_CALL(mock_delegate_, OnUserNameFetchFailure(_));
  signin_helper_->StartUserNameFetch();
  MockFailedGetAccountInfoResponse404();
}

TEST_F(WalletSigninHelperTest, AutomaticSigninSuccessful) {
  EXPECT_CALL(mock_delegate_, OnAutomaticSigninSuccess("user@gmail.com"));
  signin_helper_->StartAutomaticSignin("123SID", "123LSID");
  MockSuccessfulGaiaUserInfoResponse("user@gmail.com");
  MockSuccessfulOAuthLoginResponse();
  MockSuccessfulPassiveAuthUrlMergeAndRedirectResponse();
}

TEST_F(WalletSigninHelperTest, AutomaticSigninFailedGetUserInfo) {
  EXPECT_CALL(mock_delegate_, OnAutomaticSigninFailure(_));
  signin_helper_->StartAutomaticSignin("123SID", "123LSID");
  MockFailedGaiaUserInfoResponse404();
}

TEST_F(WalletSigninHelperTest, AutomaticSigninFailedOAuthLogin) {
  EXPECT_CALL(mock_delegate_, OnAutomaticSigninFailure(_));
  signin_helper_->StartAutomaticSignin("123SID", "123LSID");
  MockSuccessfulGaiaUserInfoResponse("user@gmail.com");
  MockFailedOAuthLoginResponse404();
}

TEST_F(WalletSigninHelperTest, AutomaticSigninFailedSignin) {
  EXPECT_CALL(mock_delegate_, OnAutomaticSigninFailure(_));
  signin_helper_->StartAutomaticSignin("123SID", "123LSID");
  MockSuccessfulGaiaUserInfoResponse("user@gmail.com");
  MockSuccessfulOAuthLoginResponse();
  MockFailedPassiveAuthUrlMergeAndRedirectResponse404();
}

// TODO(aruslan): http://crbug.com/188317 Need more tests.

}  // namespace wallet
}  // namespace autofill
