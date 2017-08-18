// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/sent_get_operation_cleanup_task.h"

#include <string>

#include "base/time/time.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/task_test_base.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
class TestingPrefetchNetworkRequestFactory
    : public PrefetchNetworkRequestFactory {
 public:
  TestingPrefetchNetworkRequestFactory() {
    ongoing_operation_names_ = base::MakeUnique<std::set<std::string>>();
  }
  ~TestingPrefetchNetworkRequestFactory() override = default;

  // PrefetchNetworkRequestFactory implementation.
  bool HasOutstandingRequests() const override { return false; }
  void MakeGeneratePageBundleRequest(
      const std::vector<std::string>& prefetch_urls,
      const std::string& gcm_registration_id,
      const PrefetchRequestFinishedCallback& callback) override {}
  std::unique_ptr<std::set<std::string>> GetAllUrlsRequested() const override {
    return std::unique_ptr<std::set<std::string>>();
  }
  void MakeGetOperationRequest(
      const std::string& operation_name,
      const PrefetchRequestFinishedCallback& callback) override {}
  GetOperationRequest* FindGetOperationRequestByName(
      const std::string& operation_name) const override {
    return nullptr;
  }
  std::unique_ptr<std::set<std::string>> GetAllOperationNamesRequested()
      const override {
    return base::MakeUnique<std::set<std::string>>(*ongoing_operation_names_);
  }

  void AddOngoingOperation(const std::string& operation_name) {
    ongoing_operation_names_->insert(operation_name);
  }

 private:
  std::unique_ptr<std::set<std::string>> ongoing_operation_names_;
};
}  // namespace

class SentGetOperationCleanupTaskTest : public TaskTestBase {
 public:
  SentGetOperationCleanupTaskTest() = default;
  ~SentGetOperationCleanupTaskTest() override = default;
};

TEST_F(SentGetOperationCleanupTaskTest, Retry) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::SENT_GET_OPERATION);
  item.get_operation_attempts =
      SentGetOperationCleanupTask::kMaxGetOperationAttempts - 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  SentGetOperationCleanupTask task(store(), prefetch_request_factory());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::RECEIVED_GCM, store_item->state);
  EXPECT_EQ(item.get_operation_attempts, store_item->get_operation_attempts);
}

TEST_F(SentGetOperationCleanupTaskTest, NoRetryForOngoingRequest) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::SENT_GET_OPERATION);
  item.get_operation_attempts =
      SentGetOperationCleanupTask::kMaxGetOperationAttempts - 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  std::unique_ptr<TestingPrefetchNetworkRequestFactory> request_factory =
      base::MakeUnique<TestingPrefetchNetworkRequestFactory>();
  request_factory->AddOngoingOperation(item.operation_name);

  SentGetOperationCleanupTask task(store(), request_factory.get());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::SENT_GET_OPERATION, store_item->state);
  EXPECT_EQ(item.get_operation_attempts, store_item->get_operation_attempts);
}

TEST_F(SentGetOperationCleanupTaskTest, ErrorOnMaxAttempts) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::SENT_GET_OPERATION);
  item.get_operation_attempts =
      SentGetOperationCleanupTask::kMaxGetOperationAttempts;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  SentGetOperationCleanupTask task(store(), prefetch_request_factory());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item->state);
  EXPECT_EQ(PrefetchItemErrorCode::GET_OPERATION_MAX_ATTEMPTS_REACHED,
            store_item->error_code);
  EXPECT_EQ(item.get_operation_attempts, store_item->get_operation_attempts);
}

TEST_F(SentGetOperationCleanupTaskTest, SkipForOngoingRequestWithMaxAttempts) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::SENT_GET_OPERATION);
  item.get_operation_attempts =
      SentGetOperationCleanupTask::kMaxGetOperationAttempts;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  std::unique_ptr<TestingPrefetchNetworkRequestFactory> request_factory =
      base::MakeUnique<TestingPrefetchNetworkRequestFactory>();
  request_factory->AddOngoingOperation(item.operation_name);

  SentGetOperationCleanupTask task(store(), request_factory.get());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::SENT_GET_OPERATION, store_item->state);
  EXPECT_EQ(item.get_operation_attempts, store_item->get_operation_attempts);
}

TEST_F(SentGetOperationCleanupTaskTest, NoUpdateForOtherStates) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::NEW_REQUEST);
  item.get_operation_attempts =
      SentGetOperationCleanupTask::kMaxGetOperationAttempts;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  SentGetOperationCleanupTask task(store(), prefetch_request_factory());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::NEW_REQUEST, store_item->state);
  EXPECT_EQ(item.get_operation_attempts, store_item->get_operation_attempts);
}

}  // namespace offline_pages
