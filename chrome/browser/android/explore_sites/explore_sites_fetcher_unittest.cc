// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_fetcher.h"

#include <map>

#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "net/http/http_request_headers.h"
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

namespace {
const char kAcceptLanguages[] = "en-US,en;q=0.5";
const char kCountryOverride[] = "country_override";
const char kExperimentData[] = "FooBar";
const char kTestData[] = "Any data.";
}  // namespace

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
  ExploreSitesRequestStatus RunFetcherWithBackoffs(
      bool is_immediate_fetch,
      size_t num_of_backoffs,
      std::vector<base::TimeDelta> backoff_delays,
      std::vector<base::OnceCallback<void(void)>> respond_callbacks,
      std::string* data_received);

  void RespondWithData(const std::string& data);
  void RespondWithNetError(int net_error);
  void RespondWithHttpError(net::HttpStatusCode http_error);

  void SetUpExperimentOption(std::string option, std::string data) {
    const std::string kTrialName = "trial_name";
    const std::string kGroupName = "group_name";

    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

    std::map<std::string, std::string> params = {{option, data}};
    base::AssociateFieldTrialParams(kTrialName, kGroupName, params);

    std::unique_ptr<base::FeatureList> feature_list =
        std::make_unique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        chrome::android::kExploreSites.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  network::ResourceRequest last_resource_request;

 private:
  ExploreSitesFetcher::Callback StoreResult();
  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest(
      size_t index);
  std::unique_ptr<ExploreSitesFetcher> CreateFetcher(bool disable_retry,
                                                     bool is_immediate_fetch);
  ExploreSitesRequestStatus RunFetcher(
      base::OnceCallback<void(void)> respond_callback,
      std::string* data_received);

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  ExploreSitesRequestStatus last_status_;
  std::unique_ptr<std::string> last_data_;
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
        EXPECT_TRUE(request.url.is_valid() && !request.url.is_empty());
        last_resource_request = request;
      }));
  field_trial_list_ = std::make_unique<base::FieldTrialList>(
      std::make_unique<base::MockEntropyProvider>());
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
      CreateFetcher(true /* disable_retry*/, true /*is_immediate_fetch*/);

  std::move(respond_callback).Run();
  task_runner_->RunUntilIdle();

  if (last_data_)
    *data_received = *last_data_;
  return last_status_;
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcherWithBackoffs(
    bool is_immediate_fetch,
    size_t num_of_backoffs,
    std::vector<base::TimeDelta> backoff_delays,
    std::vector<base::OnceCallback<void(void)>> respond_callbacks,
    std::string* data_received) {
  DCHECK_EQ(num_of_backoffs, backoff_delays.size());
  DCHECK(num_of_backoffs <= respond_callbacks.size() &&
         respond_callbacks.size() <= num_of_backoffs + 1);

  std::unique_ptr<ExploreSitesFetcher> fetcher =
      CreateFetcher(false /* disable_retry*/, is_immediate_fetch);

  std::move(respond_callbacks[0]).Run();
  task_runner_->RunUntilIdle();

  for (size_t i = 0; i < num_of_backoffs; ++i) {
    task_runner_->FastForwardBy(backoff_delays[i]);
    if (i + 1 <= respond_callbacks.size() - 1)
      std::move(respond_callbacks[i + 1]).Run();
    task_runner_->RunUntilIdle();
  }

  if (last_data_)
    *data_received = *last_data_;
  return last_status_;
}

std::unique_ptr<ExploreSitesFetcher> ExploreSitesFetcherTest::CreateFetcher(
    bool disable_retry,
    bool is_immediate_fetch) {
  std::unique_ptr<ExploreSitesFetcher> fetcher =
      ExploreSitesFetcher::CreateForGetCatalog(
          is_immediate_fetch, "123", kAcceptLanguages,
          test_shared_url_loader_factory_, StoreResult());
  if (disable_retry)
    fetcher->disable_retry_for_testing();
  fetcher->Start();
  return fetcher;
}

TEST_F(ExploreSitesFetcherTest, NetErrors) {
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldSuspendBlockedByAdministrator,
            RunFetcherWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR));

  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_INTERNET_DISCONNECTED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_NETWORK_CHANGED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_CONNECTION_RESET));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_CONNECTION_CLOSED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_CONNECTION_REFUSED));
}

TEST_F(ExploreSitesFetcherTest, HttpErrors) {
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldSuspendBadRequest,
            RunFetcherWithHttpError(net::HTTP_BAD_REQUEST));

  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_NOT_IMPLEMENTED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_NOT_FOUND));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_CONFLICT));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_BAD_GATEWAY));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_SERVICE_UNAVAILABLE));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_GATEWAY_TIMEOUT));
}

TEST_F(ExploreSitesFetcherTest, EmptyResponse) {
  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure, RunFetcherWithData("", &data));
  EXPECT_TRUE(data.empty());
}

TEST_F(ExploreSitesFetcherTest, Success) {
  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kSuccess,
            RunFetcherWithData(kTestData, &data));
  EXPECT_EQ(kTestData, data);

  EXPECT_EQ(last_resource_request.url.spec(),
            "https://exploresites-pa.googleapis.com/v1/"
            "getcatalog?country_code=DEFAULT&version_token=123");
}

TEST_F(ExploreSitesFetcherTest, DefaultCountry) {
  SetUpExperimentOption(kCountryOverride, "KZ");
  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kSuccess,
            RunFetcherWithData(kTestData, &data));
  EXPECT_EQ(kTestData, data);

  EXPECT_EQ(last_resource_request.url.spec(),
            "https://exploresites-pa.googleapis.com/v1/"
            "getcatalog?country_code=KZ&version_token=123");
}

TEST_F(ExploreSitesFetcherTest, TestHeaders) {
  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kSuccess,
            RunFetcherWithData(kTestData, &data));

  net::HttpRequestHeaders headers = last_resource_request.headers;
  std::string content_type;
  std::string languages;
  bool success;

  success = headers.HasHeader("x-goog-api-key");
  EXPECT_TRUE(success);

  success = headers.HasHeader("X-Client-Version");
  EXPECT_TRUE(success);

  success =
      headers.GetHeader(net::HttpRequestHeaders::kContentType, &content_type);
  EXPECT_TRUE(success);
  EXPECT_EQ(content_type, "application/x-protobuf");

  success =
      headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage, &languages);
  EXPECT_TRUE(success);
  EXPECT_EQ(languages, kAcceptLanguages);

  // The finch header should not be set since the experiment is not on.
  success = headers.HasHeader("X-Google-Chrome-Experiment-Tag");
  EXPECT_FALSE(success);
}

TEST_F(ExploreSitesFetcherTest, TestFinchHeader) {
  // Set up the Finch experiment.
  SetUpExperimentOption("exp", kExperimentData);

  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kSuccess,
            RunFetcherWithData(kTestData, &data));

  net::HttpRequestHeaders headers = last_resource_request.headers;
  std::string header_text;
  bool success;

  success = headers.GetHeader("X-Google-Chrome-Experiment-Tag", &header_text);
  EXPECT_EQ(std::string(kExperimentData), header_text);
}

TEST_F(ExploreSitesFetcherTest, OneBackoffForImmediateFetch) {
  std::string data;
  int initial_delay_ms =
      ExploreSitesFetcher::kImmediateFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays = {
      base::TimeDelta::FromMilliseconds(initial_delay_ms)};
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                     base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithData,
                     base::Unretained(this), kTestData));
  EXPECT_EQ(
      ExploreSitesRequestStatus::kSuccess,
      RunFetcherWithBackoffs(true /*is_immediate_fetch*/, 1u, backoff_delays,
                             std::move(respond_callbacks), &data));
  EXPECT_EQ(kTestData, data);
}

TEST_F(ExploreSitesFetcherTest, OneBackoffForBackgroundFetch) {
  std::string data;
  int initial_delay_ms =
      ExploreSitesFetcher::kBackgroundFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays = {
      base::TimeDelta::FromMilliseconds(initial_delay_ms)};
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                     base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithData,
                     base::Unretained(this), kTestData));
  EXPECT_EQ(
      ExploreSitesRequestStatus::kSuccess,
      RunFetcherWithBackoffs(false /*is_immediate_fetch*/, 1u, backoff_delays,
                             std::move(respond_callbacks), &data));
  EXPECT_EQ(kTestData, data);
}

TEST_F(ExploreSitesFetcherTest, TwoBackoffsForImmediateFetch) {
  std::string data;
  int initial_delay_ms =
      ExploreSitesFetcher::kImmediateFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays = {
      base::TimeDelta::FromMilliseconds(initial_delay_ms),
      base::TimeDelta::FromMilliseconds(initial_delay_ms * 2)};
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                     base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithHttpError,
                     base::Unretained(this), net::HTTP_INTERNAL_SERVER_ERROR));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithData,
                     base::Unretained(this), kTestData));
  EXPECT_EQ(
      ExploreSitesRequestStatus::kSuccess,
      RunFetcherWithBackoffs(true /*is_immediate_fetch*/, 2u, backoff_delays,
                             std::move(respond_callbacks), &data));
  EXPECT_EQ(kTestData, data);
}

TEST_F(ExploreSitesFetcherTest, TwoBackoffsForBackgroundFetch) {
  std::string data;
  int initial_delay_ms =
      ExploreSitesFetcher::kBackgroundFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays = {
      base::TimeDelta::FromMilliseconds(initial_delay_ms),
      base::TimeDelta::FromMilliseconds(initial_delay_ms * 2)};
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                     base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithHttpError,
                     base::Unretained(this), net::HTTP_INTERNAL_SERVER_ERROR));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithData,
                     base::Unretained(this), kTestData));
  EXPECT_EQ(
      ExploreSitesRequestStatus::kSuccess,
      RunFetcherWithBackoffs(false /*is_immediate_fetch*/, 2u, backoff_delays,
                             std::move(respond_callbacks), &data));
  EXPECT_EQ(kTestData, data);
}

TEST_F(ExploreSitesFetcherTest, ExceedMaxBackoffsForImmediateFetch) {
  std::string data;
  int delay_ms =
      ExploreSitesFetcher::kImmediateFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays;
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  for (int i = 0; i < ExploreSitesFetcher::kMaxFailureCountForImmediateFetch;
       ++i) {
    backoff_delays.push_back(base::TimeDelta::FromMilliseconds(delay_ms));
    delay_ms *= 2;
    respond_callbacks.push_back(
        base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                       base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  }
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithBackoffs(
                true /*is_immediate_fetch*/,
                ExploreSitesFetcher::kMaxFailureCountForImmediateFetch,
                backoff_delays, std::move(respond_callbacks), &data));
  EXPECT_TRUE(data.empty());
}

TEST_F(ExploreSitesFetcherTest, ExceedMaxBackoffsForBackgroundFetch) {
  std::string data;
  int delay_ms =
      ExploreSitesFetcher::kBackgroundFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays;
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  for (int i = 0; i < ExploreSitesFetcher::kMaxFailureCountForBackgroundFetch;
       ++i) {
    backoff_delays.push_back(base::TimeDelta::FromMilliseconds(delay_ms));
    delay_ms *= 2;
    respond_callbacks.push_back(
        base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                       base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  }
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithBackoffs(
                false /*is_immediate_fetch*/,
                ExploreSitesFetcher::kMaxFailureCountForBackgroundFetch,
                backoff_delays, std::move(respond_callbacks), &data));
  EXPECT_TRUE(data.empty());
}

}  // namespace explore_sites
