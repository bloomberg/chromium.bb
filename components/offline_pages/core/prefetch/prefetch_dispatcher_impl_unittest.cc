// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/prefetch/test_prefetch_network_request_factory.h"
#include "components/version_info/channel.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

const std::string kTestNamespace = "TestPrefetchClientNamespace";

class TestScopedBackgroundTask
    : public PrefetchDispatcher::ScopedBackgroundTask {
 public:
  TestScopedBackgroundTask() = default;
  ~TestScopedBackgroundTask() override = default;

  void SetNeedsReschedule(bool reschedule, bool backoff) override {
    needs_reschedule_called = true;
  }

  bool needs_reschedule_called = false;
};

}  // namespace

class PrefetchDispatcherTest : public testing::Test {
 public:
  PrefetchDispatcherTest();

  // Test implementation.
  void SetUp() override;
  void TearDown() override;

  void PumpLoop();

  PrefetchDispatcher::ScopedBackgroundTask* GetBackgroundTask() {
    return dispatcher_->background_task_.get();
  }

  TaskQueue* dispatcher_task_queue() { return &dispatcher_->task_queue_; }
  PrefetchDispatcher* prefetch_dispatcher() { return dispatcher_; }

  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }

 protected:
  std::vector<PrefetchURL> test_urls_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<PrefetchServiceTestTaco> taco_;

  base::test::ScopedFeatureList feature_list_;

  // Owned by |taco_|.
  PrefetchDispatcherImpl* dispatcher_;
};

PrefetchDispatcherTest::PrefetchDispatcherTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {
  feature_list_.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);
}

void PrefetchDispatcherTest::SetUp() {
  dispatcher_ = new PrefetchDispatcherImpl();
  taco_.reset(new PrefetchServiceTestTaco);
  taco_->SetPrefetchDispatcher(base::WrapUnique(dispatcher_));
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

void PrefetchDispatcherTest::PumpLoop() {
  task_runner()->RunUntilIdle();
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
  PrefetchURL prefetch_url("id", GURL("https://www.chromium.org"),
                           base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kTestNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());

  // Do nothing with a new background task.
  prefetch_dispatcher()->BeginBackgroundTask(
      base::MakeUnique<TestScopedBackgroundTask>());
  EXPECT_EQ(nullptr, GetBackgroundTask());

  // Everything else is unimplemented.
}

}  // namespace offline_pages
