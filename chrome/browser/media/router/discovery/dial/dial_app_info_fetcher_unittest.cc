// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_info_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media_router {

class DialAppInfoFetcherTest : public testing::Test {
 public:
  DialAppInfoFetcherTest() : url_("http://127.0.0.1/app/Youtube") {}

  void TearDown() override {
    EXPECT_FALSE(error_cb_);
    EXPECT_FALSE(success_cb_);
  }

  void ExpectSuccess(const std::string& expected_app_info) {
    success_cb_ = base::BindOnce(&DialAppInfoFetcherTest::OnSuccess,
                                 base::Unretained(this), expected_app_info);
  }

  void ExpectError(const std::string& expected_message) {
    error_cb_ = base::BindOnce(&DialAppInfoFetcherTest::OnError,
                               base::Unretained(this), expected_message);
  }

  net::TestURLFetcher* StartRequest() {
    fetcher_ = base::MakeUnique<DialAppInfoFetcher>(
        url_, profile_.GetRequestContext(), std::move(success_cb_),
        std::move(error_cb_));
    fetcher_->Start();
    return factory_.GetFetcherByID(DialAppInfoFetcher::kURLFetcherIDForTest);
  }

 protected:
  const content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  const net::TestURLFetcherFactory factory_;
  const GURL url_;
  base::OnceCallback<void(const std::string&)> success_cb_;
  base::OnceCallback<void(int, const std::string&)> error_cb_;
  std::unique_ptr<DialAppInfoFetcher> fetcher_;

 private:
  void OnSuccess(const std::string& expected_app_info,
                 const std::string& app_info) {
    EXPECT_EQ(expected_app_info, app_info);
  }

  void OnError(const std::string& expected_message,
               int response_code,
               const std::string& message) {
    EXPECT_TRUE(message.find(expected_message) == 0);
  }

  DISALLOW_COPY_AND_ASSIGN(DialAppInfoFetcherTest);
};

TEST_F(DialAppInfoFetcherTest, FetchSuccessful) {
  ExpectSuccess("<xml>appInfo</xml>");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  test_fetcher->SetResponseString("<xml>appInfo</xml>");
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DialAppInfoFetcherTest, FetchFailsOnMissingAppInfo) {
  ExpectError("HTTP 404:");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_NOT_FOUND);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DialAppInfoFetcherTest, FetchFailsOnEmptyAppInfo) {
  ExpectError("Missing or empty response");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  test_fetcher->SetResponseString("");
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DialAppInfoFetcherTest, FetchFailsOnBadAppInfo) {
  ExpectError("Invalid response encoding");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  test_fetcher->SetResponseString("\xfc\x9c\xbf\x80\xbf\x80");
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DialAppInfoFetcherTest, FetchFailsOnResponseTooLarge) {
  ExpectError("Response too large");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  test_fetcher->SetResponseString(std::string(262145, 'd'));
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

}  // namespace media_router
