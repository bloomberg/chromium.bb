// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/metrics_finalization_task.h"

#include <set>

#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/prefetch/mock_prefetch_item_generator.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/task_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class MetricsFinalizationTaskTest : public TaskTestBase {
 public:
  MetricsFinalizationTaskTest() = default;
  ~MetricsFinalizationTaskTest() override = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<MetricsFinalizationTask> metrics_finalization_task_;
};

void MetricsFinalizationTaskTest::SetUp() {
  TaskTestBase::SetUp();
  metrics_finalization_task_ =
      base::MakeUnique<MetricsFinalizationTask>(store());
}

void MetricsFinalizationTaskTest::TearDown() {
  metrics_finalization_task_.reset();
  TaskTestBase::TearDown();
}

// Tests that the task works correctly with an empty database.
TEST_F(MetricsFinalizationTaskTest, EmptyRun) {
  EXPECT_EQ(0, store_util()->CountPrefetchItems());

  // Execute the metrics task.
  ExpectTaskCompletes(metrics_finalization_task_.get());
  metrics_finalization_task_->Run();
  RunUntilIdle();
  EXPECT_EQ(0, store_util()->CountPrefetchItems());
}

// Verifies that expired and non-expired items from all expirable states are
// properly handled.
TEST_F(MetricsFinalizationTaskTest, LeavesOtherStatesAlone) {
  // Insert fresh and stale items for all expirable states from all buckets.
  std::vector<PrefetchItemState> all_states_but_finished = {
      PrefetchItemState::NEW_REQUEST,
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE,
      PrefetchItemState::AWAITING_GCM,
      PrefetchItemState::RECEIVED_GCM,
      PrefetchItemState::SENT_GET_OPERATION,
      PrefetchItemState::RECEIVED_BUNDLE,
      PrefetchItemState::DOWNLOADING,
      PrefetchItemState::DOWNLOADED,
      PrefetchItemState::IMPORTING,
      PrefetchItemState::ZOMBIE,
  };

  for (auto& state : all_states_but_finished) {
    PrefetchItem item = item_generator()->CreateItem(state);
    EXPECT_TRUE(store_util()->InsertPrefetchItem(item))
        << "Failed inserting item with state " << static_cast<int>(state);
  }

  std::set<PrefetchItem> all_inserted_items;
  EXPECT_EQ(10U, store_util()->GetAllItems(&all_inserted_items));

  // Execute the task.
  ExpectTaskCompletes(metrics_finalization_task_.get());
  metrics_finalization_task_->Run();
  RunUntilIdle();

  std::set<PrefetchItem> all_items_after_task;
  EXPECT_EQ(10U, store_util()->GetAllItems(&all_items_after_task));
  EXPECT_EQ(all_inserted_items, all_items_after_task);
}

TEST_F(MetricsFinalizationTaskTest, FinalizesMultipleItems) {
  std::set<PrefetchItem> finished_items = {
      item_generator()->CreateItem(PrefetchItemState::FINISHED),
      item_generator()->CreateItem(PrefetchItemState::FINISHED),
      item_generator()->CreateItem(PrefetchItemState::FINISHED)};

  PrefetchItem unfinished_item =
      item_generator()->CreateItem(PrefetchItemState::NEW_REQUEST);

  for (auto& item : finished_items) {
    ASSERT_TRUE(store_util()->InsertPrefetchItem(item));
  }
  ASSERT_TRUE(store_util()->InsertPrefetchItem(unfinished_item));

  // Execute the metrics task.
  ExpectTaskCompletes(metrics_finalization_task_.get());
  metrics_finalization_task_->Run();
  RunUntilIdle();

  std::set<PrefetchItem> all_items;
  // The finished ones should be zombies and the new request should be
  // untouched.
  EXPECT_EQ(4U, store_util()->GetAllItems(&all_items));
  EXPECT_EQ(0U, FilterByState(all_items, PrefetchItemState::FINISHED).size());
  EXPECT_EQ(3U, FilterByState(all_items, PrefetchItemState::ZOMBIE).size());

  std::set<PrefetchItem> items_in_new_request_state =
      FilterByState(all_items, PrefetchItemState::NEW_REQUEST);
  EXPECT_EQ(1U, items_in_new_request_state.count(unfinished_item));
}

}  // namespace offline_pages
