// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/experimental/safe_search_url_reporter.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAccountId[] = "account@gmail.com";

}  // namespace

class SafeSearchURLReporterTest : public testing::Test {
 public:
  SafeSearchURLReporterTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        report_url_(&token_service_, kAccountId, request_context_.get()) {
    token_service_.UpdateCredentials(kAccountId, "refresh_token");
  }

 protected:
  void IssueAccessTokens() {
    token_service_.IssueAllTokensForAccount(
        kAccountId, "access_token",
        base::Time::Now() + base::TimeDelta::FromHours(1));
  }

  void IssueAccessTokenErrors() {
    token_service_.IssueErrorForAllPendingRequestsForAccount(
        kAccountId, GoogleServiceAuthError::FromServiceError("Error!"));
  }

  void CreateRequest(int url_fetcher_id, const GURL& url) {
    report_url_.set_url_fetcher_id_for_testing(url_fetcher_id);
    report_url_.ReportUrl(
        url, base::BindOnce(&SafeSearchURLReporterTest::OnRequestCreated,
                            base::Unretained(this)));
  }

  net::TestURLFetcher* GetURLFetcher(int id) {
    net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(id);
    EXPECT_TRUE(url_fetcher);
    return url_fetcher;
  }

  void SendResponse(int url_fetcher_id, net::Error error) {
    net::TestURLFetcher* url_fetcher = GetURLFetcher(url_fetcher_id);
    url_fetcher->set_status(net::URLRequestStatus::FromError(error));
    url_fetcher->set_response_code(net::HTTP_OK);
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void SendValidResponse(int url_fetcher_id) {
    SendResponse(url_fetcher_id, net::OK);
  }

  void SendFailedResponse(int url_fetcher_id) {
    SendResponse(url_fetcher_id, net::ERR_ABORTED);
  }

  MOCK_METHOD1(OnRequestCreated, void(bool success));

  base::MessageLoop message_loop_;
  FakeProfileOAuth2TokenService token_service_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  SafeSearchURLReporter report_url_;
};

TEST_F(SafeSearchURLReporterTest, Success) {
  CreateRequest(0, GURL("http://google.com"));
  CreateRequest(1, GURL("http://url.com"));

  EXPECT_GT(token_service_.GetPendingRequests().size(), 0U);

  IssueAccessTokens();

  EXPECT_CALL(*this, OnRequestCreated(true));
  SendValidResponse(0);
  EXPECT_CALL(*this, OnRequestCreated(true));
  SendValidResponse(1);
}

TEST_F(SafeSearchURLReporterTest, AccessTokenError) {
  CreateRequest(0, GURL("http://google.com"));

  EXPECT_EQ(1U, token_service_.GetPendingRequests().size());

  EXPECT_CALL(*this, OnRequestCreated(false));
  IssueAccessTokenErrors();
}

TEST_F(SafeSearchURLReporterTest, NetworkError) {
  CreateRequest(0, GURL("http://google.com"));

  EXPECT_EQ(1U, token_service_.GetPendingRequests().size());

  IssueAccessTokens();

  EXPECT_CALL(*this, OnRequestCreated(false));
  SendFailedResponse(0);
}
