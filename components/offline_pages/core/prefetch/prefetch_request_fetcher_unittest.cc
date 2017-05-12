// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_request_fetcher.h"

#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::SaveArg;

namespace offline_pages {

namespace {
const GURL kTestUrl("http://example.com");
const char kTestMessage[] = "Testing";
}  // namespace

class PrefetchRequestFetcherTest : public PrefetchRequestTestBase {
 public:
  PrefetchRequestStatus RunFetcherWithNetError(net::Error net_error);
  PrefetchRequestStatus RunFetcherWithHttpError(int http_error);
  PrefetchRequestStatus RunFetcherWithData(const std::string& response_data,
                                           std::string* data_received);
};

PrefetchRequestStatus PrefetchRequestFetcherTest::RunFetcherWithNetError(
    net::Error net_error) {
  base::MockCallback<PrefetchRequestFetcher::FinishedCallback> callback;
  std::unique_ptr<PrefetchRequestFetcher> fetcher(new PrefetchRequestFetcher(
      kTestUrl, kTestMessage, request_context(), callback.Get()));

  PrefetchRequestStatus status;
  std::string data;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&data)));
  RespondWithNetError(net_error);

  EXPECT_TRUE(data.empty());
  return status;
}

PrefetchRequestStatus PrefetchRequestFetcherTest::RunFetcherWithHttpError(
    int http_error) {
  base::MockCallback<PrefetchRequestFetcher::FinishedCallback> callback;
  std::unique_ptr<PrefetchRequestFetcher> fetcher(new PrefetchRequestFetcher(
      kTestUrl, kTestMessage, request_context(), callback.Get()));

  PrefetchRequestStatus status;
  std::string data;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&data)));
  RespondWithHttpError(http_error);

  EXPECT_TRUE(data.empty());
  return status;
}

PrefetchRequestStatus PrefetchRequestFetcherTest::RunFetcherWithData(
    const std::string& response_data,
    std::string* data_received) {
  base::MockCallback<PrefetchRequestFetcher::FinishedCallback> callback;
  std::unique_ptr<PrefetchRequestFetcher> fetcher(new PrefetchRequestFetcher(
      kTestUrl, kTestMessage, request_context(), callback.Get()));

  PrefetchRequestStatus status;
  std::string data;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&data)));
  RespondWithData(response_data);

  *data_received = data;
  return status;
}

TEST_F(PrefetchRequestFetcherTest, NetErrors) {
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_SUSPEND,
            RunFetcherWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR));

  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_INTERNET_DISCONNECTED));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_NETWORK_CHANGED));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_CONNECTION_RESET));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_CONNECTION_CLOSED));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITHOUT_BACKOFF,
            RunFetcherWithNetError(net::ERR_CONNECTION_REFUSED));
}

TEST_F(PrefetchRequestFetcherTest, HttpErrors) {
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_SUSPEND,
            RunFetcherWithHttpError(net::HTTP_NOT_IMPLEMENTED));

  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_BAD_REQUEST));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_NOT_FOUND));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_CONFLICT));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_BAD_GATEWAY));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_SERVICE_UNAVAILABLE));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithHttpError(net::HTTP_GATEWAY_TIMEOUT));
}

TEST_F(PrefetchRequestFetcherTest, EmptyResponse) {
  std::string data;
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            RunFetcherWithData("", &data));
  EXPECT_TRUE(data.empty());
}

TEST_F(PrefetchRequestFetcherTest, Success) {
  std::string data;
  EXPECT_EQ(PrefetchRequestStatus::SUCCESS,
            RunFetcherWithData("Any data.", &data));
  EXPECT_FALSE(data.empty());
}

}  // namespace offline_pages
