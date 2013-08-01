// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_signin_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/content/browser/wallet/wallet_signin_helper_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    "{\"user_info\":["
    "  {"
    "    \"email\": \"%s\""
    "  }"
    "]}";

class MockWalletSigninHelperDelegate : public WalletSigninHelperDelegate {
 public:
  MOCK_METHOD1(OnPassiveSigninSuccess, void(const std::string& username));
  MOCK_METHOD1(OnUserNameFetchSuccess, void(const std::string& username));
  MOCK_METHOD1(OnPassiveSigninFailure,
               void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnUserNameFetchFailure,
               void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnDidFetchWalletCookieValue,
               void(const std::string& cookie_value));
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
 protected:
  virtual void SetUp() OVERRIDE {
    signin_helper_.reset(new WalletSigninHelperForTesting(
        &mock_delegate_,
        browser_context_.GetRequestContext()));
    EXPECT_EQ(WalletSigninHelperForTesting::IDLE, state());
  }

  virtual void TearDown() OVERRIDE {
    signin_helper_.reset();
  }

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

  void MockSuccessfulGetAccountInfoResponse(const std::string& username) {
    SetUpFetcherResponseAndCompleteRequest(
        signin_helper_->GetGetAccountInfoUrlForTesting(), net::HTTP_OK,
        net::ResponseCookies(),
        base::StringPrintf(
            kGetAccountInfoValidResponseFormat,
            username.c_str()));
  }

  void MockFailedGetAccountInfoResponse404() {
    SetUpFetcherResponseAndCompleteRequest(
        signin_helper_->GetGetAccountInfoUrlForTesting(),
        net::HTTP_NOT_FOUND,
        net::ResponseCookies(),
        std::string());
  }

  void MockSuccessfulPassiveSignInResponse() {
    SetUpFetcherResponseAndCompleteRequest(wallet::GetPassiveAuthUrl().spec(),
                                           net::HTTP_OK,
                                           net::ResponseCookies(),
                                           "YES");
  }

  void MockFailedPassiveSignInResponseNo() {
    SetUpFetcherResponseAndCompleteRequest(wallet::GetPassiveAuthUrl().spec(),
                                           net::HTTP_OK,
                                           net::ResponseCookies(),
                                           "NOOOOOOOOOOOOOOO");
  }

  void MockFailedPassiveSignInResponse404() {
    SetUpFetcherResponseAndCompleteRequest(wallet::GetPassiveAuthUrl().spec(),
                                           net::HTTP_NOT_FOUND,
                                           net::ResponseCookies(),
                                           std::string());
  }

  WalletSigninHelperForTesting::State state() const {
    return signin_helper_->state();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<WalletSigninHelperForTesting> signin_helper_;
  MockWalletSigninHelperDelegate mock_delegate_;
  TestingProfile browser_context_;

 private:
  net::TestURLFetcherFactory factory_;
};

TEST_F(WalletSigninHelperTest, PassiveSigninSuccessful) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninSuccess("user@gmail.com"));
  signin_helper_->StartPassiveSignin();
  MockSuccessfulPassiveSignInResponse();
  MockSuccessfulGetAccountInfoResponse("user@gmail.com");
}

TEST_F(WalletSigninHelperTest, PassiveSigninFailedSignin404) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninFailure(_));
  signin_helper_->StartPassiveSignin();
  MockFailedPassiveSignInResponse404();
}

TEST_F(WalletSigninHelperTest, PassiveSigninFailedSigninNo) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninFailure(_));
  signin_helper_->StartPassiveSignin();
  MockFailedPassiveSignInResponseNo();
}

TEST_F(WalletSigninHelperTest, PassiveSigninFailedUserInfo) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninFailure(_));
  signin_helper_->StartPassiveSignin();
  MockSuccessfulPassiveSignInResponse();
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

TEST_F(WalletSigninHelperTest, GetWalletCookieValueWhenPresent) {
  EXPECT_CALL(mock_delegate_, OnDidFetchWalletCookieValue("gdToken"));
  net::CookieMonster* cookie_monster = new net::CookieMonster(NULL, NULL);
  net::CookieOptions httponly_options;
  httponly_options.set_include_httponly();
  scoped_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::Create(GetPassiveAuthUrl().GetWithEmptyPath(),
                                   "gdToken=gdToken; HttpOnly",
                                   base::Time::Now(),
                                   httponly_options));

  net::CookieList cookie_list;
  cookie_list.push_back(*cookie);
  cookie_monster->InitializeFrom(cookie_list);
  browser_context_.GetRequestContext()->GetURLRequestContext()
      ->set_cookie_store(cookie_monster);
  signin_helper_->StartWalletCookieValueFetch();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WalletSigninHelperTest, GetWalletCookieValueWhenMissing) {
  EXPECT_CALL(mock_delegate_, OnDidFetchWalletCookieValue(std::string()));
  net::CookieMonster* cookie_monster = new net::CookieMonster(NULL, NULL);
  net::CookieOptions httponly_options;
  httponly_options.set_include_httponly();
  scoped_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::Create(GetPassiveAuthUrl().GetWithEmptyPath(),
                                   "fake_cookie=monkeys; HttpOnly",
                                   base::Time::Now(),
                                   httponly_options));

  net::CookieList cookie_list;
  cookie_list.push_back(*cookie);
  cookie_monster->InitializeFrom(cookie_list);
  browser_context_.GetRequestContext()->GetURLRequestContext()
      ->set_cookie_store(cookie_monster);
  signin_helper_->StartWalletCookieValueFetch();
  base::RunLoop().RunUntilIdle();
}

// TODO(aruslan): http://crbug.com/188317 Need more tests.

}  // namespace wallet
}  // namespace autofill
