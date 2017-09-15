// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/offline_pages/core/prefetch/prefetch_configuration.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/prefetch/test_prefetch_network_request_factory.h"
#include "components/version_info/channel.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Contains;

namespace offline_pages {

namespace {

const std::string kTestNamespace = "TestPrefetchClientNamespace";

class TestScopedBackgroundTask
    : public PrefetchDispatcher::ScopedBackgroundTask {
 public:
  TestScopedBackgroundTask() = default;
  TestScopedBackgroundTask(base::RepeatingCallback<void(bool, bool)> callback)
      : callback_(std::move(callback)) {}
  ~TestScopedBackgroundTask() override = default;

  void SetNeedsReschedule(bool reschedule, bool backoff) override {
    if (!callback_.is_null())
      callback_.Run(reschedule, backoff);
  }

 private:
  base::RepeatingCallback<void(bool, bool)> callback_;
};

}  // namespace

class PrefetchDispatcherTest : public PrefetchRequestTestBase {
 public:
  PrefetchDispatcherTest();

  // Test implementation.
  void SetUp() override;
  void TearDown() override;

  PrefetchDispatcher::ScopedBackgroundTask* GetBackgroundTask() {
    return dispatcher_->background_task_.get();
  }

  TaskQueue& GetTaskQueueFrom(PrefetchDispatcherImpl* prefetch_dispatcher) {
    return prefetch_dispatcher->task_queue_;
  }

  void SetNeedsReschedule(bool reschedule, bool backoff) {
    needs_reschedule_called_ = true;
    reschedule_result_ = reschedule;
    backoff_result_ = backoff;
  }

  TaskQueue* dispatcher_task_queue() { return &dispatcher_->task_queue_; }
  PrefetchDispatcher* prefetch_dispatcher() { return dispatcher_; }
  TestPrefetchNetworkRequestFactory* network_request_factory() {
    return network_request_factory_;
  }

  bool needs_reschedule_called() const { return needs_reschedule_called_; }
  bool reschedule_result() const { return reschedule_result_; }
  bool backoff_result() const { return backoff_result_; }

 protected:
  std::vector<PrefetchURL> test_urls_;

 private:
  std::unique_ptr<PrefetchServiceTestTaco> taco_;

  base::test::ScopedFeatureList feature_list_;

  // Owned by |taco_|.
  PrefetchDispatcherImpl* dispatcher_;
  // Owned by |taco_|.
  TestPrefetchNetworkRequestFactory* network_request_factory_;

  bool needs_reschedule_called_ = false;
  bool reschedule_result_ = false;
  bool backoff_result_ = false;
};

PrefetchDispatcherTest::PrefetchDispatcherTest() {
  feature_list_.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);
}

void PrefetchDispatcherTest::SetUp() {
  dispatcher_ = new PrefetchDispatcherImpl();
  network_request_factory_ = new TestPrefetchNetworkRequestFactory();
  taco_.reset(new PrefetchServiceTestTaco);
  taco_->SetPrefetchDispatcher(base::WrapUnique(dispatcher_));
  taco_->SetPrefetchNetworkRequestFactory(
      base::WrapUnique(network_request_factory_));
  taco_->CreatePrefetchService();

  ASSERT_TRUE(test_urls_.empty());
  test_urls_.push_back({"1", GURL("http://testurl.com/foo"), base::string16()});
  test_urls_.push_back(
      {"2", GURL("https://testurl.com/bar"), base::string16()});
}

void PrefetchDispatcherTest::TearDown() {
  // Ensures that the store is properly disposed off.
  taco_.reset();
  PumpLoop();
}

TEST_F(PrefetchDispatcherTest, DispatcherDoesNotCrash) {
  // TODO(https://crbug.com/735254): Ensure that Dispatcher unit test keep up
  // with the state of adding tasks, and that the end state is we have tests
  // that verify the proper tasks were added in the proper order at each wakeup
  // signal of the dispatcher.
  prefetch_dispatcher()->AddCandidatePrefetchURLs(kTestNamespace, test_urls_);
  prefetch_dispatcher()->RemoveAllUnprocessedPrefetchURLs(
      kSuggestedArticlesNamespace);
  prefetch_dispatcher()->RemovePrefetchURLsByClientId(
      {kSuggestedArticlesNamespace, "123"});
}

TEST_F(PrefetchDispatcherTest, AddCandidatePrefetchURLsTask) {
  prefetch_dispatcher()->AddCandidatePrefetchURLs(kTestNamespace, test_urls_);
  EXPECT_TRUE(dispatcher_task_queue()->HasPendingTasks());
  PumpLoop();
  EXPECT_FALSE(dispatcher_task_queue()->HasPendingTasks());
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());
}

TEST_F(PrefetchDispatcherTest, DispatcherDoesNothingIfFeatureNotEnabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kPrefetchingOfflinePagesFeature);

  // Don't add a task for new prefetch URLs.
  prefetch_dispatcher()->AddCandidatePrefetchURLs(kTestNamespace, test_urls_);
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());

  // Do nothing with a new background task.
  prefetch_dispatcher()->BeginBackgroundTask(
      base::MakeUnique<TestScopedBackgroundTask>());
  EXPECT_EQ(nullptr, GetBackgroundTask());

  // TODO(carlosk): add more checks here.
}

class DisablingPrefetchConfiguration : public PrefetchConfiguration {
 public:
  bool IsPrefetchingEnabledBySettings() override { return false; };
};

TEST_F(PrefetchDispatcherTest, DispatcherDoesNothingIfSettingsDoNotAllowIt) {
  PrefetchDispatcherImpl* dispatcher = new PrefetchDispatcherImpl();
  PrefetchServiceTestTaco taco;
  taco.SetPrefetchDispatcher(base::WrapUnique(dispatcher));
  taco.SetPrefetchNetworkRequestFactory(
      base::MakeUnique<TestPrefetchNetworkRequestFactory>());
  taco.SetPrefetchConfiguration(
      base::MakeUnique<DisablingPrefetchConfiguration>());
  taco.CreatePrefetchService();

  // Don't add a task for new prefetch URLs.
  dispatcher->AddCandidatePrefetchURLs(kTestNamespace, test_urls_);
  EXPECT_FALSE(GetTaskQueueFrom(dispatcher).HasRunningTask());

  // Do nothing with a new background task.
  dispatcher->BeginBackgroundTask(base::MakeUnique<TestScopedBackgroundTask>());
  EXPECT_EQ(nullptr, GetBackgroundTask());

  // TODO(carlosk): add more checks here.
}

TEST_F(PrefetchDispatcherTest, DispatcherReleasesBackgroundTask) {
  PrefetchURL prefetch_url("id", GURL("https://www.chromium.org"),
                           base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kTestNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  PumpLoop();

  // We start the background task, causing reconcilers and action tasks to be
  // run. We should hold onto the background task until there is no more work to
  // do, after the network request ends.
  ASSERT_EQ(nullptr, GetBackgroundTask());
  prefetch_dispatcher()->BeginBackgroundTask(
      base::MakeUnique<TestScopedBackgroundTask>());
  EXPECT_TRUE(dispatcher_task_queue()->HasRunningTask());
  PumpLoop();

  // Still holding onto the background task.
  EXPECT_NE(nullptr, GetBackgroundTask());
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());
  EXPECT_THAT(*network_request_factory()->GetAllUrlsRequested(),
              Contains(prefetch_url.url.spec()));

  // When the network request finishes, the dispatcher should still hold the
  // ScopedBackgroundTask because it needs to process the results of the
  // request.
  RespondWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_NE(nullptr, GetBackgroundTask());
  PumpLoop();

  // Because there is no work remaining, the background task should be released.
  EXPECT_EQ(nullptr, GetBackgroundTask());
}

TEST_F(PrefetchDispatcherTest, RetryWithBackoffAfterFailedNetworkRequest) {
  PrefetchURL prefetch_url("id", GURL("https://www.chromium.org"),
                           base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kTestNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  PumpLoop();

  prefetch_dispatcher()->BeginBackgroundTask(
      base::MakeUnique<TestScopedBackgroundTask>(
          base::BindRepeating(&PrefetchDispatcherTest::SetNeedsReschedule,
                              base::Unretained(this))));
  PumpLoop();

  // This should trigger retry with backoff.
  RespondWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR);
  PumpLoop();

  EXPECT_TRUE(needs_reschedule_called());
  EXPECT_TRUE(reschedule_result());
  EXPECT_TRUE(backoff_result());
}

TEST_F(PrefetchDispatcherTest, RetryWithoutBackoffAfterFailedNetworkRequest) {
  PrefetchURL prefetch_url("id", GURL("https://www.chromium.org"),
                           base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kTestNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  PumpLoop();

  prefetch_dispatcher()->BeginBackgroundTask(
      base::MakeUnique<TestScopedBackgroundTask>(
          base::BindRepeating(&PrefetchDispatcherTest::SetNeedsReschedule,
                              base::Unretained(this))));
  PumpLoop();

  // This should trigger retry without backoff.
  RespondWithNetError(net::ERR_CONNECTION_CLOSED);
  PumpLoop();

  EXPECT_TRUE(needs_reschedule_called());
  EXPECT_TRUE(reschedule_result());
  EXPECT_FALSE(backoff_result());
}

}  // namespace offline_pages
