// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/generate_page_bundle_reconcile_task.h"

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
class FakePrefetchNetworkRequestFactory : public PrefetchNetworkRequestFactory {
 public:
  FakePrefetchNetworkRequestFactory() {
    requested_urls_ = base::MakeUnique<std::set<std::string>>();
  }
  ~FakePrefetchNetworkRequestFactory() override = default;

  // Implementation of PrefetchNetworkRequestFactory
  bool HasOutstandingRequests() const override { return false; }
  void MakeGeneratePageBundleRequest(
      const std::vector<std::string>& prefetch_urls,
      const std::string& gcm_registration_id,
      const PrefetchRequestFinishedCallback& callback) override {}
  std::unique_ptr<std::set<std::string>> GetAllUrlsRequested() const override {
    return base::MakeUnique<std::set<std::string>>(*requested_urls_);
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
    return nullptr;
  }

  void AddRequestedUrl(const std::string& url) { requested_urls_->insert(url); }

 private:
  std::unique_ptr<std::set<std::string>> requested_urls_;
};
}  // namespace

class GeneratePageBundleReconcileTaskTest : public TaskTestBase {
 public:
  GeneratePageBundleReconcileTaskTest();
  ~GeneratePageBundleReconcileTaskTest() override = default;

  FakePrefetchNetworkRequestFactory* request_factory() {
    return request_factory_.get();
  }

  // Inserts the item with specified fields set into database, returns the
  // same item.
  PrefetchItem InsertItem(PrefetchItemState state, int attempts_count);

 private:
  std::unique_ptr<FakePrefetchNetworkRequestFactory> request_factory_;
};

GeneratePageBundleReconcileTaskTest::GeneratePageBundleReconcileTaskTest()
    : request_factory_(base::MakeUnique<FakePrefetchNetworkRequestFactory>()) {}

PrefetchItem GeneratePageBundleReconcileTaskTest::InsertItem(
    PrefetchItemState state,
    int attempts_count) {
  PrefetchItem item = item_generator()->CreateItem(state);
  item.generate_bundle_attempts = attempts_count;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
  return item;
}

TEST_F(GeneratePageBundleReconcileTaskTest, Retry) {
  PrefetchItem item = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  item.generate_bundle_attempts =
      GeneratePageBundleReconcileTask::kMaxGenerateBundleAttempts - 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  GeneratePageBundleReconcileTask task(store(), request_factory());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::NEW_REQUEST, store_item->state);
  EXPECT_EQ(item.generate_bundle_attempts,
            store_item->generate_bundle_attempts);
}

TEST_F(GeneratePageBundleReconcileTaskTest, NoRetryForOngoingRequest) {
  PrefetchItem item = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  item.generate_bundle_attempts =
      GeneratePageBundleReconcileTask::kMaxGenerateBundleAttempts - 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  request_factory()->AddRequestedUrl(item.url.spec());

  GeneratePageBundleReconcileTask task(store(), request_factory());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  EXPECT_EQ(item, *store_item);
}

TEST_F(GeneratePageBundleReconcileTaskTest, ErrorOnMaxAttempts) {
  PrefetchItem item = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  item.generate_bundle_attempts =
      GeneratePageBundleReconcileTask::kMaxGenerateBundleAttempts;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  GeneratePageBundleReconcileTask task(store(), request_factory());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item->state);
  EXPECT_EQ(
      PrefetchItemErrorCode::GENERATE_PAGE_BUNDLE_REQUEST_MAX_ATTEMPTS_REACHED,
      store_item->error_code);
  EXPECT_EQ(item.generate_bundle_attempts,
            store_item->generate_bundle_attempts);
}

TEST_F(GeneratePageBundleReconcileTaskTest,
       SkipForOngoingRequestWithMaxAttempts) {
  PrefetchItem item = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  item.generate_bundle_attempts =
      GeneratePageBundleReconcileTask::kMaxGenerateBundleAttempts;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  request_factory()->AddRequestedUrl(item.url.spec());

  GeneratePageBundleReconcileTask task(store(), request_factory());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  EXPECT_EQ(item, *store_item);
}

TEST_F(GeneratePageBundleReconcileTaskTest, NoUpdateForOtherStates) {
  std::set<PrefetchItem> items;
  const int attempts_count =
      GeneratePageBundleReconcileTask::kMaxGenerateBundleAttempts;
  // Add items in all states other than SENT_GENERATE_PAGE_BUNDLE.
  items.insert(InsertItem(PrefetchItemState::NEW_REQUEST, attempts_count));
  items.insert(InsertItem(PrefetchItemState::AWAITING_GCM, attempts_count));
  items.insert(InsertItem(PrefetchItemState::RECEIVED_GCM, attempts_count));
  items.insert(
      InsertItem(PrefetchItemState::SENT_GET_OPERATION, attempts_count));
  items.insert(InsertItem(PrefetchItemState::RECEIVED_BUNDLE, attempts_count));
  items.insert(InsertItem(PrefetchItemState::DOWNLOADING, attempts_count));
  items.insert(InsertItem(PrefetchItemState::DOWNLOADED, attempts_count));
  items.insert(InsertItem(PrefetchItemState::IMPORTING, attempts_count));
  items.insert(InsertItem(PrefetchItemState::FINISHED, attempts_count));
  items.insert(InsertItem(PrefetchItemState::ZOMBIE, attempts_count));

  GeneratePageBundleReconcileTask task(store(), request_factory());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::set<PrefetchItem> store_items;
  store_util()->GetAllItems(&store_items);
  EXPECT_EQ(items, store_items);
}

}  // namespace offline_pages