// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chrome/browser/local_discovery/cloud_print_account_manager.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_discovery {

namespace {

const char kSampleResponse[] = "{"
    "   \"success\": true,"
    "   \"xsrf_token\": \"sample\","
    "   \"request\" : { "
    "     \"users\": [\"first@gmail.com\", \"second@gmail.com\"]"
    "   } "
    "}";

const char kSampleResponseFailure[] = "{"
    "   \"success\": false,"
    "}";

class MockCallback {
 public:
  MOCK_METHOD2(CloudPrintAccountsResolved, void(
      const std::vector<std::string>& account,
      const std::string& xsrf_token));
};

class CloudPrintAccountManagerTest : public testing::Test {
 public:
  CloudPrintAccountManagerTest()
      : request_context_(
            new net::TestURLRequestContextGetter(
                base::MessageLoopProxy::current())),
        account_manager_(
            request_context_.get(),
            "https://www.google.com/cloudprint",
            1,
            base::Bind(
                &MockCallback::CloudPrintAccountsResolved,
                base::Unretained(&mock_callback_))) {
  }

  virtual ~CloudPrintAccountManagerTest() {
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory fetcher_factory_;
  MockCallback mock_callback_;
  CloudPrintAccountManager account_manager_;
};

TEST_F(CloudPrintAccountManagerTest, Success) {
  account_manager_.Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);

  net::HttpRequestHeaders headers;
  std::string proxy;
  fetcher->GetExtraRequestHeaders(&headers);
  EXPECT_TRUE(headers.GetHeader("X-Cloudprint-Proxy", &proxy));
  EXPECT_EQ("Chrome", proxy);
  EXPECT_EQ(GURL("https://www.google.com/cloudprint/list?proxy=none&user=1"),
            fetcher->GetOriginalURL());

  fetcher->SetResponseString(kSampleResponse);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  std::vector<std::string> expected_users;
  expected_users.push_back("first@gmail.com");
  expected_users.push_back("second@gmail.com");

  EXPECT_CALL(mock_callback_,
              CloudPrintAccountsResolved(expected_users, "sample"));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(CloudPrintAccountManagerTest, FailureJSON) {
  account_manager_.Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);

  fetcher->SetResponseString(kSampleResponseFailure);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(mock_callback_,
              CloudPrintAccountsResolved(std::vector<std::string>(), ""));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

}  // namespace

}  // namespace local_discovery
