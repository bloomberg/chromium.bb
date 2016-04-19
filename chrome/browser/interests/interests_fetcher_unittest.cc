// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interests/interests_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock-generated-function-mockers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock-more-matchers.h"
#include "testing/gmock/include/gmock/gmock-spec-builders.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;
using testing::Not;

namespace {

const int kInterestsFetcherURLFetcherID = 0;
const char kInterestsURL[] = "http://www.fake.com";
const char kEmptyResponse[] =
    "{\n"
    "  \"interests\": []\n"
    "}\n";
const char kSuccessfulResponse[] =
    "{\n"
    "  \"interests\": [\n"
    "    {\n"
    "      \"name\": \"Google\",\n"
    "      \"imageUrl\": \"https://fake.com/fake.png\",\n"
    "      \"relevance\": 0.9\n"
    "    },\n"
    "    {\n"
    "      \"name\": \"Google Chrome\",\n"
    "      \"imageUrl\": \"https://fake.com/fake.png\",\n"
    "      \"relevance\": 0.98\n"
    "    }\n"
    "  ]\n"
    "}\n";
const char kAccountId[] = "account@gmail.com";

std::vector<InterestsFetcher::Interest> GetExpectedEmptyResponse() {
  return std::vector<InterestsFetcher::Interest>();
}

std::vector<InterestsFetcher::Interest> GetExpectedSuccessfulResponse() {
  std::vector<InterestsFetcher::Interest> res;
  res.push_back(InterestsFetcher::Interest{
      "Google", GURL("https://fake.com/fake.png"), 0.9});
  res.push_back(InterestsFetcher::Interest{
      "Google Chrome", GURL("https://fake.com/fake.png"), 0.98});
  return res;
}
}  // namespace

class InterestsFetcherTest : public testing::Test {
 public:
  InterestsFetcherTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        url_fetcher_factory_(new net::TestURLFetcherFactory()) {
    token_service_.UpdateCredentials(kAccountId, "refresh_token");

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    command_line->AppendSwitchASCII(switches::kInterestsURL, kInterestsURL);
  }

  MOCK_METHOD0(OnSuccessfulResponse, void());
  MOCK_METHOD0(OnEmptyResponse, void());
  MOCK_METHOD0(OnFailedResponse, void());

  void OnReceivedInterests(
      std::unique_ptr<std::vector<InterestsFetcher::Interest>> interests) {
    if (!interests) {
      OnFailedResponse();
      return;
    }

    if (*interests == GetExpectedEmptyResponse())
      OnEmptyResponse();
    else if (*interests == GetExpectedSuccessfulResponse())
      OnSuccessfulResponse();
    else
      FAIL() << "Unexpected interests response.";
  }

 protected:
  void RequestInterests() {
    request_.reset(new InterestsFetcher(&token_service_,
                                        kAccountId,
                                        request_context_.get()));

    request_->FetchInterests(base::Bind(
        &InterestsFetcherTest::OnReceivedInterests, base::Unretained(this)));
  }

  net::TestURLFetcher* GetURLFetcher() {
    net::TestURLFetcher* url_fetcher =
        url_fetcher_factory_->GetFetcherByID(kInterestsFetcherURLFetcherID);
    EXPECT_TRUE(url_fetcher);
    return url_fetcher;
  }

  void IssueAccessTokenErrors() {
    token_service_.IssueErrorForAllPendingRequestsForAccount(
        kAccountId, GoogleServiceAuthError::FromServiceError("Error!"));
  }

  void IssueAccessTokens() {
    token_service_.IssueAllTokensForAccount(
        kAccountId, "access_token",
        base::Time::Now() + base::TimeDelta::FromHours(1));
  }

  void SendResponse(net::Error error,
                    int response_code,
                    const std::string& response) {
    net::TestURLFetcher* url_fetcher = GetURLFetcher();
    url_fetcher->set_status(net::URLRequestStatus::FromError(error));
    url_fetcher->set_response_code(response_code);
    url_fetcher->SetResponseString(response);
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void SendValidResponse(const std::string& response) {
    SendResponse(net::OK, net::HTTP_OK, response);
  }

  void SendFailedResponse() {
    SendResponse(net::ERR_ABORTED, 0, std::string());
  }

  void SendAuthorizationError() {
    SendResponse(net::OK, net::HTTP_UNAUTHORIZED, std::string());
  }

  base::MessageLoop message_loop_;
  FakeProfileOAuth2TokenService token_service_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  std::unique_ptr<net::TestURLFetcherFactory> url_fetcher_factory_;
  std::unique_ptr<InterestsFetcher> request_;
};

TEST_F(InterestsFetcherTest, EmptyResponse) {
  RequestInterests();
  EXPECT_CALL(*this, OnEmptyResponse());
  IssueAccessTokens();
  SendValidResponse(kEmptyResponse);
}

TEST_F(InterestsFetcherTest, SuccessfullResponse) {
  RequestInterests();
  EXPECT_CALL(*this, OnSuccessfulResponse());
  IssueAccessTokens();
  SendValidResponse(kSuccessfulResponse);
}

TEST_F(InterestsFetcherTest, FailedResponse) {
  RequestInterests();
  EXPECT_CALL(*this, OnFailedResponse());
  IssueAccessTokens();
  SendFailedResponse();
}

TEST_F(InterestsFetcherTest, FailedOAuthRequest) {
  RequestInterests();
  EXPECT_CALL(*this, OnFailedResponse());
  IssueAccessTokenErrors();
}

TEST_F(InterestsFetcherTest, RetryOnAuthorizationError) {
  RequestInterests();

  EXPECT_CALL(*this, OnEmptyResponse()).Times(0);
  IssueAccessTokens();
  SendAuthorizationError();

  EXPECT_CALL(*this, OnEmptyResponse());
  IssueAccessTokens();
  SendValidResponse(kEmptyResponse);
}

TEST_F(InterestsFetcherTest, RetryOnlyOnceOnAuthorizationError) {
  RequestInterests();

  EXPECT_CALL(*this, OnEmptyResponse()).Times(0);
  IssueAccessTokens();
  SendAuthorizationError();

  EXPECT_CALL(*this, OnFailedResponse());
  IssueAccessTokens();
  SendAuthorizationError();
}
