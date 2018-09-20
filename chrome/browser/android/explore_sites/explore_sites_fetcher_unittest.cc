// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_fetcher.h"

#include "base/message_loop/message_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::SaveArg;

namespace explore_sites {

// TODO(freedjm): Add tests for the headers.
class ExploreSitesFetcherTest : public testing::Test {
 public:
  ExploreSitesFetcherTest();

  void SetUp() override;

  ExploreSitesRequestStatus RunFetcherWithNetError(net::Error net_error);
  ExploreSitesRequestStatus RunFetcherWithHttpError(
      net::HttpStatusCode http_error);
  ExploreSitesRequestStatus RunFetcherWithData(const std::string& response_data,
                                               std::string* data_received);

 private:
  ExploreSitesFetcher::Callback StoreResult();
  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest(
      size_t index);
  ExploreSitesRequestStatus RunFetcher(
      base::OnceCallback<void(void)> respond_callback,
      std::string* data_received);
  void RespondWithData(const std::string& data);
  void RespondWithNetError(int net_error);
  void RespondWithHttpError(net::HttpStatusCode http_error);

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::ResourceRequest last_resource_request_;

  ExploreSitesRequestStatus last_status_;
  std::unique_ptr<std::string> last_data_;
  base::MessageLoopForIO message_loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

ExploreSitesFetcher::Callback ExploreSitesFetcherTest::StoreResult() {
  return base::BindLambdaForTesting(
      [&](ExploreSitesRequestStatus status, std::unique_ptr<std::string> data) {
        last_status_ = status;
        last_data_ = std::move(data);
      });
}

ExploreSitesFetcherTest::ExploreSitesFetcherTest()
    : test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      task_runner_(new base::TestMockTimeTaskRunner) {
  message_loop_.SetTaskRunner(task_runner_);
}

void ExploreSitesFetcherTest::SetUp() {
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        last_resource_request_ = request;
      }));
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcherWithNetError(
    net::Error net_error) {
  std::string data_received;
  ExploreSitesRequestStatus status =
      RunFetcher(base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                                base::Unretained(this), net_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());
  return status;
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcherWithHttpError(
    net::HttpStatusCode http_error) {
  std::string data_received;
  ExploreSitesRequestStatus status =
      RunFetcher(base::BindOnce(&ExploreSitesFetcherTest::RespondWithHttpError,
                                base::Unretained(this), http_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());
  return status;
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcherWithData(
    const std::string& response_data,
    std::string* data_received) {
  return RunFetcher(base::BindOnce(&ExploreSitesFetcherTest::RespondWithData,
                                   base::Unretained(this), response_data),
                    data_received);
}

void ExploreSitesFetcherTest::RespondWithNetError(int net_error) {
  int pending_requests_count = test_url_loader_factory_.NumPending();
  DCHECK(pending_requests_count > 0);
  network::URLLoaderCompletionStatus completion_status(net_error);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingRequest(0)->request.url, completion_status,
      network::ResourceResponseHead(), std::string());
}

void ExploreSitesFetcherTest::RespondWithHttpError(
    net::HttpStatusCode http_error) {
  int pending_requests_count = test_url_loader_factory_.NumPending();
  auto resource_response_head = network::CreateResourceResponseHead(http_error);
  DCHECK(pending_requests_count > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingRequest(0)->request.url,
      network::URLLoaderCompletionStatus(net::OK), resource_response_head,
      std::string());
}

void ExploreSitesFetcherTest::RespondWithData(const std::string& data) {
  DCHECK(test_url_loader_factory_.pending_requests()->size() > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingRequest(0)->request.url.spec(), data);
}

network::TestURLLoaderFactory::PendingRequest*
ExploreSitesFetcherTest::GetPendingRequest(size_t index) {
  if (index >= test_url_loader_factory_.pending_requests()->size())
    return nullptr;
  network::TestURLLoaderFactory::PendingRequest* request =
      &(*test_url_loader_factory_.pending_requests())[index];
  DCHECK(request);
  return request;
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcher(
    base::OnceCallback<void(void)> respond_callback,
    std::string* data_received) {
  std::unique_ptr<ExploreSitesFetcher> fetcher =
      ExploreSitesFetcher::CreateForGetCatalog(StoreResult(), 0, "KE",
                                               test_shared_url_loader_factory_);

  std::move(respond_callback).Run();
  task_runner_->RunUntilIdle();

  if (last_data_)
    *data_received = *last_data_;
  return last_status_;
}

TEST_F(ExploreSitesFetcherTest, NetErrors) {
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldSuspendBlockedByAdministrator,
            RunFetcherWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR));

  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithNetError(net::ERR_INTERNET_DISCONNECTED));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithNetError(net::ERR_NETWORK_CHANGED));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithNetError(net::ERR_CONNECTION_RESET));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithNetError(net::ERR_CONNECTION_CLOSED));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithNetError(net::ERR_CONNECTION_REFUSED));
}

TEST_F(ExploreSitesFetcherTest, HttpErrors) {
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldSuspendBadRequest,
            RunFetcherWithHttpError(net::HTTP_BAD_REQUEST));

  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithHttpError(net::HTTP_NOT_IMPLEMENTED));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithHttpError(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithHttpError(net::HTTP_NOT_FOUND));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithHttpError(net::HTTP_CONFLICT));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithHttpError(net::HTTP_BAD_GATEWAY));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithHttpError(net::HTTP_SERVICE_UNAVAILABLE));
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithHttpError(net::HTTP_GATEWAY_TIMEOUT));
}

TEST_F(ExploreSitesFetcherTest, EmptyResponse) {
  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldRetry,
            RunFetcherWithData("", &data));
  EXPECT_TRUE(data.empty());
}

TEST_F(ExploreSitesFetcherTest, Success) {
  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kSuccess,
            RunFetcherWithData("Any data.", &data));
  EXPECT_FALSE(data.empty());
  EXPECT_EQ(data, "Any data.");
}

}  // namespace explore_sites
