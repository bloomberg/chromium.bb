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
#include "components/offline_pages/core/prefetch/prefetch_in_memory_store.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

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

class PrefetchDispatcherTest : public testing::Test, public PrefetchService {
 public:
  PrefetchDispatcherTest();

  // Test implementation.
  void SetUp() override;
  void TearDown() override;

  // PrefetchService implementation:
  OfflineEventLogger* GetLogger() override;
  OfflineMetricsCollector* GetOfflineMetricsCollector() override;
  PrefetchDispatcher* GetPrefetchDispatcher() override;
  PrefetchGCMHandler* GetPrefetchGCMHandler() override;
  PrefetchStore* GetPrefetchStore() override;
  SuggestedArticlesObserver* GetSuggestedArticlesObserver() override;

  // KeyedService implementation.
  void Shutdown() override {}

  void PumpLoop();
  PrefetchDispatcher::ScopedBackgroundTask* GetBackgroundTask() {
    return dispatcher_impl_->task_.get();
  }

  TaskQueue* dispatcher_task_queue() { return &dispatcher_impl_->task_queue_; }

 private:
  OfflineEventLogger logger_;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<PrefetchInMemoryStore> in_memory_store_;
  std::unique_ptr<PrefetchDispatcherImpl> dispatcher_impl_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

PrefetchDispatcherTest::PrefetchDispatcherTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {
  feature_list_.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);
}

void PrefetchDispatcherTest::SetUp() {
  ASSERT_EQ(base::ThreadTaskRunnerHandle::Get(), task_runner_);
  ASSERT_FALSE(task_runner_->HasPendingTask());
  in_memory_store_ = base::MakeUnique<PrefetchInMemoryStore>();
  dispatcher_impl_ = base::MakeUnique<PrefetchDispatcherImpl>();
  dispatcher_impl_->SetService(this);
}

void PrefetchDispatcherTest::TearDown() {
  task_runner_->ClearPendingTasks();
}

OfflineEventLogger* PrefetchDispatcherTest::GetLogger() {
  return &logger_;
}

OfflineMetricsCollector* PrefetchDispatcherTest::GetOfflineMetricsCollector() {
  NOTREACHED();
  return nullptr;
}

PrefetchDispatcher* PrefetchDispatcherTest::GetPrefetchDispatcher() {
  return dispatcher_impl_.get();
}

PrefetchGCMHandler* PrefetchDispatcherTest::GetPrefetchGCMHandler() {
  NOTREACHED();
  return nullptr;
}

PrefetchStore* PrefetchDispatcherTest::GetPrefetchStore() {
  return in_memory_store_.get();
}

SuggestedArticlesObserver*
PrefetchDispatcherTest::GetSuggestedArticlesObserver() {
  NOTREACHED();
  return nullptr;
}

void PrefetchDispatcherTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

TEST_F(PrefetchDispatcherTest, DispatcherDoesNotCrash) {
  GetPrefetchDispatcher()->AddCandidatePrefetchURLs(std::vector<PrefetchURL>());
  GetPrefetchDispatcher()->RemoveAllUnprocessedPrefetchURLs(
      kSuggestedArticlesNamespace);
  GetPrefetchDispatcher()->RemovePrefetchURLsByClientId(
      {kSuggestedArticlesNamespace, "123"});
}

TEST_F(PrefetchDispatcherTest, AddCandidatePrefetchURLsTask) {
  GetPrefetchDispatcher()->AddCandidatePrefetchURLs(std::vector<PrefetchURL>());
  EXPECT_TRUE(dispatcher_task_queue()->HasPendingTasks());
  EXPECT_TRUE(dispatcher_task_queue()->HasRunningTask());
  PumpLoop();
  EXPECT_FALSE(dispatcher_task_queue()->HasPendingTasks());
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());
}

TEST_F(PrefetchDispatcherTest, DispatcherDoesNothingIfFeatureNotEnabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kPrefetchingOfflinePagesFeature);

  // Don't add a task for new prefetch URLs.
  ClientId client_id("namespace", "id");
  PrefetchURL prefetch_url(client_id, GURL("https://www.chromium.org"));
  GetPrefetchDispatcher()->AddCandidatePrefetchURLs(
      std::vector<PrefetchURL>(1, prefetch_url));
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());

  // Do nothing with a new background task.
  GetPrefetchDispatcher()->BeginBackgroundTask(
      base::MakeUnique<TestScopedBackgroundTask>());
  EXPECT_EQ(nullptr, GetBackgroundTask());

  // Everything else is unimplemented.
}

}  // namespace offline_pages
