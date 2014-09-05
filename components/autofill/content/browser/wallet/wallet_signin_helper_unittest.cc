// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_signin_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/content/browser/wallet/wallet_signin_helper_delegate.h"
#include "content/public/browser/cookie_store_factory.h"
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
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace autofill {
namespace wallet {

namespace {

class MockWalletSigninHelperDelegate : public WalletSigninHelperDelegate {
 public:
  MOCK_METHOD0(OnPassiveSigninSuccess, void());
  MOCK_METHOD1(OnPassiveSigninFailure,
               void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnDidFetchWalletCookieValue,
               void(const std::string& cookie_value));
};

}  // namespace

class WalletSigninHelperTest : public testing::Test {
 protected:
  WalletSigninHelperTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())) {}
  virtual ~WalletSigninHelperTest() {}

  virtual void SetUp() OVERRIDE {
    signin_helper_.reset(
        new WalletSigninHelper(&mock_delegate_, request_context_.get()));
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

  void MockSuccessfulPassiveSignInResponse() {
    SetUpFetcherResponseAndCompleteRequest(wallet::GetPassiveAuthUrl(0).spec(),
                                           net::HTTP_OK,
                                           net::ResponseCookies(),
                                           "YES");
  }

  void MockFailedPassiveSignInResponseNo() {
    SetUpFetcherResponseAndCompleteRequest(wallet::GetPassiveAuthUrl(0).spec(),
                                           net::HTTP_OK,
                                           net::ResponseCookies(),
                                           "NOOOOOOOOOOOOOOO");
  }

  void MockFailedPassiveSignInResponse404() {
    SetUpFetcherResponseAndCompleteRequest(wallet::GetPassiveAuthUrl(0).spec(),
                                           net::HTTP_NOT_FOUND,
                                           net::ResponseCookies(),
                                           std::string());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<WalletSigninHelper> signin_helper_;
  MockWalletSigninHelperDelegate mock_delegate_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;

 private:
  net::TestURLFetcherFactory factory_;
};

TEST_F(WalletSigninHelperTest, PassiveSigninSuccessful) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninSuccess());
  signin_helper_->StartPassiveSignin(0);
  MockSuccessfulPassiveSignInResponse();
}

TEST_F(WalletSigninHelperTest, PassiveSigninFailedSignin404) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninFailure(_));
  signin_helper_->StartPassiveSignin(0);
  MockFailedPassiveSignInResponse404();
}

TEST_F(WalletSigninHelperTest, PassiveSigninFailedSigninNo) {
  EXPECT_CALL(mock_delegate_, OnPassiveSigninFailure(_));
  signin_helper_->StartPassiveSignin(0);
  MockFailedPassiveSignInResponseNo();
}

TEST_F(WalletSigninHelperTest, GetWalletCookieValueWhenPresent) {
  EXPECT_CALL(mock_delegate_, OnDidFetchWalletCookieValue("gdToken"));
  net::CookieMonster* cookie_monster =
      content::CreateCookieStore(content::CookieStoreConfig())->
          GetCookieMonster();
  net::CookieOptions httponly_options;
  httponly_options.set_include_httponly();
  scoped_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::Create(GetPassiveAuthUrl(0).GetWithEmptyPath(),
                                   "gdToken=gdToken; HttpOnly",
                                   base::Time::Now(),
                                   httponly_options));

  net::CookieList cookie_list;
  cookie_list.push_back(*cookie);
  cookie_monster->ImportCookies(cookie_list);
  request_context_->GetURLRequestContext()
      ->set_cookie_store(cookie_monster);
  signin_helper_->StartWalletCookieValueFetch();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WalletSigninHelperTest, GetWalletCookieValueWhenMissing) {
  EXPECT_CALL(mock_delegate_, OnDidFetchWalletCookieValue(std::string()));
  net::CookieMonster* cookie_monster =
      content::CreateCookieStore(content::CookieStoreConfig())->
          GetCookieMonster();
  net::CookieOptions httponly_options;
  httponly_options.set_include_httponly();
  scoped_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::Create(GetPassiveAuthUrl(0).GetWithEmptyPath(),
                                   "fake_cookie=monkeys; HttpOnly",
                                   base::Time::Now(),
                                   httponly_options));

  net::CookieList cookie_list;
  cookie_list.push_back(*cookie);
  cookie_monster->ImportCookies(cookie_list);
  request_context_->GetURLRequestContext()
      ->set_cookie_store(cookie_monster);
  signin_helper_->StartWalletCookieValueFetch();
  base::RunLoop().RunUntilIdle();
}

// TODO(aruslan): http://crbug.com/188317 Need more tests.

}  // namespace wallet
}  // namespace autofill
