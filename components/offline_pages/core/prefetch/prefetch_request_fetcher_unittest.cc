// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_request_fetcher.h"

#include "base/message_loop/message_loop.h"
#include "base/test/mock_callback.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::SaveArg;

namespace offline_prefetch {

namespace {
const GURL kTestUrl("http://example.com");
const char kTestMessage[] = "Testing";
}  // namespace

class PrefetchRequestFetcherTest : public testing::Test {
 public:
  PrefetchRequestFetcherTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())) {}

  PrefetchRequestFetcher::Status RunFetcherWithNetError(net::Error net_error);
  PrefetchRequestFetcher::Status RunFetcherWithHttpError(int http_error);
  PrefetchRequestFetcher::Status RunFetcherWithData(
      const std::string& response_data,
      std::string* data_received);

 private:
  void RespondWithNetError(int net_error);
  void RespondWithHttpError(int http_error);
  void RespondWithData(const std::string& data);

  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
};

void PrefetchRequestFetcherTest::RespondWithNetError(int net_error) {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  DCHECK(url_fetcher);
  url_fetcher->set_status(net::URLRequestStatus::FromError(net_error));
  url_fetcher->SetResponseString("");
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
}

void PrefetchRequestFetcherTest::RespondWithHttpError(int http_error) {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  DCHECK(url_fetcher);
  url_fetcher->set_status(net::URLRequestStatus());
  url_fetcher->set_response_code(http_error);
  url_fetcher->SetResponseString("");
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
}

void PrefetchRequestFetcherTest::RespondWithData(const std::string& data) {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  DCHECK(url_fetcher);
  url_fetcher->set_status(net::URLRequestStatus());
  url_fetcher->set_response_code(net::HTTP_OK);
  url_fetcher->SetResponseString(data);
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
}

PrefetchRequestFetcher::Status
PrefetchRequestFetcherTest::RunFetcherWithNetError(net::Error net_error) {
  base::MockCallback<PrefetchRequestFetcher::FinishedCallback> callback;
  std::unique_ptr<PrefetchRequestFetcher> fetcher(new PrefetchRequestFetcher(
      kTestUrl, kTestMessage, request_context_, callback.Get()));

  PrefetchRequestFetcher::Status status;
  std::string data;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&data)));
  RespondWithNetError(net_error);

  EXPECT_TRUE(data.empty());
  return status;
}

PrefetchRequestFetcher::Status
PrefetchRequestFetcherTest::RunFetcherWithHttpError(int http_error) {
  base::MockCallback<PrefetchRequestFetcher::FinishedCallback> callback;
  std::unique_ptr<PrefetchRequestFetcher> fetcher(new PrefetchRequestFetcher(
      kTestUrl, kTestMessage, request_context_, callback.Get()));

  PrefetchRequestFetcher::Status status;
  std::string data;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&data)));
  RespondWithHttpError(http_error);

  EXPECT_TRUE(data.empty());
  return status;
}

PrefetchRequestFetcher::Status PrefetchRequestFetcherTest::RunFetcherWithData(
    const std::string& response_data,
    std::string* data_received) {
  base::MockCallback<PrefetchRequestFetcher::FinishedCallback> callback;
  std::unique_ptr<PrefetchRequestFetcher> fetcher(new PrefetchRequestFetcher(
      kTestUrl, kTestMessage, request_context_, callback.Get()));

  PrefetchRequestFetcher::Status status;
  std::string data;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&data)));
  RespondWithData(response_data);

  *data_received = data;
  return status;
}

TEST_F(PrefetchRequestFetcherTest, NetErrors) {
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_SUSPEND,
            RunFetcherWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR));

  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_INTERNET_DISCONNECTED));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_NETWORK_CHANGED));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_CONNECTION_RESET));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_CONNECTION_CLOSED));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_CONNECTION_REFUSED));
}

TEST_F(PrefetchRequestFetcherTest, HttpErrors) {
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_SUSPEND,
            RunFetcherWithHttpError(net::HTTP_NOT_IMPLEMENTED));

  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_BAD_REQUEST));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_NOT_FOUND));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_CONFLICT));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_BAD_GATEWAY));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_SERVICE_UNAVAILABLE));
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_GATEWAY_TIMEOUT));
}

TEST_F(PrefetchRequestFetcherTest, EmptyResponse) {
  std::string data;
  EXPECT_EQ(PrefetchRequestFetcher::Status::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithData("", &data));
  EXPECT_TRUE(data.empty());
}

TEST_F(PrefetchRequestFetcherTest, Success) {
  std::string data;
  EXPECT_EQ(PrefetchRequestFetcher::Status::SUCCESS,
            RunFetcherWithData("Any data.", &data));
  EXPECT_FALSE(data.empty());
}

}  // namespace offline_prefetch
