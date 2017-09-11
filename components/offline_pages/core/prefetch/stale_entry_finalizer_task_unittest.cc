// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/stale_entry_finalizer_task.h"

#include <set>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/mock_prefetch_item_generator.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using Result = StaleEntryFinalizerTask::Result;

std::set<PrefetchItem> Filter(const std::set<PrefetchItem>& items,
                              PrefetchItemState state) {
  std::set<PrefetchItem> result;
  for (const PrefetchItem& item : items) {
    if (item.state == state)
      result.insert(item);
  }
  return result;
}

class StaleEntryFinalizerTaskTest : public TaskTestBase {
 public:
  StaleEntryFinalizerTaskTest() = default;
  ~StaleEntryFinalizerTaskTest() override = default;

  void SetUp() override;
  void TearDown() override;

  PrefetchItem CreateAndInsertItem(PrefetchItemState state,
                                   int time_delta_in_hours);

  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }

 protected:
  TestPrefetchDispatcher dispatcher_;
  std::unique_ptr<StaleEntryFinalizerTask> stale_finalizer_task_;
  base::Time fake_now_;
};

void StaleEntryFinalizerTaskTest::SetUp() {
  TaskTestBase::SetUp();
  stale_finalizer_task_ =
      base::MakeUnique<StaleEntryFinalizerTask>(dispatcher(), store());
  fake_now_ = base::Time() + base::TimeDelta::FromDays(100);
  stale_finalizer_task_->SetNowGetterForTesting(base::BindRepeating(
      [](base::Time t) -> base::Time { return t; }, fake_now_));
}

void StaleEntryFinalizerTaskTest::TearDown() {
  stale_finalizer_task_.reset();
  TaskTestBase::TearDown();
}

PrefetchItem StaleEntryFinalizerTaskTest::CreateAndInsertItem(
    PrefetchItemState state,
    int time_delta_in_hours) {
  PrefetchItem item(item_generator()->CreateItem(state));
  item.freshness_time =
      fake_now_ + base::TimeDelta::FromHours(time_delta_in_hours);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item))
      << "Failed inserting item with state " << static_cast<int>(state);
  return item;
}

TEST_F(StaleEntryFinalizerTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  // Execute the expiration task.
  ExpectTaskCompletes(stale_finalizer_task_.get());
  stale_finalizer_task_->Run();
  RunUntilIdle();
}

// Tests that the task works correctly with an empty database.
TEST_F(StaleEntryFinalizerTaskTest, EmptyRun) {
  std::set<PrefetchItem> no_items;
  EXPECT_EQ(0U, store_util()->GetAllItems(&no_items));

  // Execute the expiration task.
  ExpectTaskCompletes(stale_finalizer_task_.get());
  stale_finalizer_task_->Run();
  RunUntilIdle();
  EXPECT_EQ(Result::NO_MORE_WORK, stale_finalizer_task_->final_status());
  EXPECT_EQ(0U, store_util()->GetAllItems(&no_items));
}

// Verifies that expired and non-expired items from all expirable states are
// properly handled.
TEST_F(StaleEntryFinalizerTaskTest, HandlesFreshnessTimesCorrectly) {
  // Insert fresh and stale items for all expirable states from all buckets.
  PrefetchItem b1_item1_fresh =
      CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, -23);
  PrefetchItem b1_item2_stale =
      CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, -25);

  PrefetchItem b2_item1_fresh =
      CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, -23);
  PrefetchItem b2_item2_stale =
      CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, -25);
  PrefetchItem b2_item3_fresh =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, -23);
  PrefetchItem b2_item4_stale =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, -25);
  PrefetchItem b2_item5_fresh =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, -23);
  PrefetchItem b2_item6_stale =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, -25);

  PrefetchItem b3_item1_fresh =
      CreateAndInsertItem(PrefetchItemState::DOWNLOADING, -47);
  PrefetchItem b3_item2_stale =
      CreateAndInsertItem(PrefetchItemState::DOWNLOADING, -49);

  // Check inserted initial items.
  std::set<PrefetchItem> initial_items = {
      b1_item1_fresh, b1_item2_stale, b2_item1_fresh, b2_item2_stale,
      b2_item3_fresh, b2_item4_stale, b2_item5_fresh, b2_item6_stale,
      b3_item1_fresh, b3_item2_stale};
  std::set<PrefetchItem> all_inserted_items;
  EXPECT_EQ(10U, store_util()->GetAllItems(&all_inserted_items));
  EXPECT_EQ(initial_items, all_inserted_items);

  // Execute the expiration task.
  ExpectTaskCompletes(stale_finalizer_task_.get());
  stale_finalizer_task_->Run();
  RunUntilIdle();
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Create the expected finished version of each stale item.
  PrefetchItem b1_item2_finished(b1_item2_stale);
  b1_item2_finished.state = PrefetchItemState::FINISHED;
  b1_item2_finished.error_code = PrefetchItemErrorCode::STALE_AT_NEW_REQUEST;
  PrefetchItem b2_item2_finished(b2_item2_stale);
  b2_item2_finished.state = PrefetchItemState::FINISHED;
  b2_item2_finished.error_code = PrefetchItemErrorCode::STALE_AT_AWAITING_GCM;
  PrefetchItem b2_item4_finished(b2_item4_stale);
  b2_item4_finished.state = PrefetchItemState::FINISHED;
  b2_item4_finished.error_code = PrefetchItemErrorCode::STALE_AT_RECEIVED_GCM;
  PrefetchItem b2_item6_finished(b2_item6_stale);
  b2_item6_finished.state = PrefetchItemState::FINISHED;
  b2_item6_finished.error_code =
      PrefetchItemErrorCode::STALE_AT_RECEIVED_BUNDLE;
  PrefetchItem b3_item2_finished(b3_item2_stale);
  b3_item2_finished.state = PrefetchItemState::FINISHED;
  b3_item2_finished.error_code = PrefetchItemErrorCode::STALE_AT_DOWNLOADING;

  // Creates the expected set of final items and compares with what's in store.
  std::set<PrefetchItem> expected_final_items = {
      b1_item1_fresh, b1_item2_finished, b2_item1_fresh, b2_item2_finished,
      b2_item3_fresh, b2_item4_finished, b2_item5_fresh, b2_item6_finished,
      b3_item1_fresh, b3_item2_finished};
  EXPECT_EQ(10U, expected_final_items.size());
  std::set<PrefetchItem> all_items_post_expiration;
  EXPECT_EQ(10U, store_util()->GetAllItems(&all_items_post_expiration));
  EXPECT_EQ(expected_final_items, all_items_post_expiration);
}

// Checks that items from all states are handled properly by the task when all
// their freshness dates are really old.
TEST_F(StaleEntryFinalizerTaskTest, HandlesStalesInAllStatesCorrectly) {
  // Insert "stale" items for every state.
  const int many_hours = -7 * 24;
  CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, many_hours);
  CreateAndInsertItem(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE, many_hours);
  CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, many_hours);
  CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, many_hours);
  CreateAndInsertItem(PrefetchItemState::SENT_GET_OPERATION, many_hours);
  CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, many_hours);
  CreateAndInsertItem(PrefetchItemState::DOWNLOADING, many_hours);
  CreateAndInsertItem(PrefetchItemState::DOWNLOADED, many_hours);
  CreateAndInsertItem(PrefetchItemState::IMPORTING, many_hours);
  CreateAndInsertItem(PrefetchItemState::FINISHED, many_hours);
  CreateAndInsertItem(PrefetchItemState::ZOMBIE, many_hours);
  EXPECT_EQ(11, store_util()->CountPrefetchItems());

  // Execute the expiration task.
  ExpectTaskCompletes(stale_finalizer_task_.get());
  stale_finalizer_task_->Run();
  RunUntilIdle();
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Checks item counts for states expected to still exist.
  std::set<PrefetchItem> post_items;
  EXPECT_EQ(11U, store_util()->GetAllItems(&post_items));
  EXPECT_EQ(
      1U,
      Filter(post_items, PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE).size());
  EXPECT_EQ(1U,
            Filter(post_items, PrefetchItemState::SENT_GET_OPERATION).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::DOWNLOADED).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::IMPORTING).size());
  EXPECT_EQ(6U, Filter(post_items, PrefetchItemState::FINISHED).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::ZOMBIE).size());
}

TEST_F(StaleEntryFinalizerTaskTest, NoWorkInQueue) {
  CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, 0);
  CreateAndInsertItem(PrefetchItemState::ZOMBIE, 0);

  ExpectTaskCompletes(stale_finalizer_task_.get());
  stale_finalizer_task_->Run();
  RunUntilIdle();
  EXPECT_EQ(Result::NO_MORE_WORK, stale_finalizer_task_->final_status());
  EXPECT_EQ(0, dispatcher()->task_schedule_count);
}

TEST_F(StaleEntryFinalizerTaskTest, WorkInQueue) {
  std::vector<PrefetchItemState> work_states = {
      PrefetchItemState::NEW_REQUEST,
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE,
      PrefetchItemState::RECEIVED_GCM,
      PrefetchItemState::SENT_GET_OPERATION,
      PrefetchItemState::RECEIVED_BUNDLE,
      PrefetchItemState::DOWNLOADING,
      PrefetchItemState::DOWNLOADED,
      PrefetchItemState::IMPORTING,
      PrefetchItemState::FINISHED};

  for (auto& state : work_states) {
    store_util()->DeleteStore();
    store_util()->BuildStoreInMemory();
    dispatcher()->task_schedule_count = 0;

    PrefetchItem item = item_generator()->CreateItem(state);
    ASSERT_TRUE(store_util()->InsertPrefetchItem(item))
        << "Failed inserting item with state " << static_cast<int>(state);

    StaleEntryFinalizerTask task(dispatcher(), store());
    ExpectTaskCompletes(&task);
    task.Run();
    RunUntilIdle();
    EXPECT_EQ(Result::MORE_WORK_NEEDED, task.final_status());

    EXPECT_EQ(1, dispatcher()->task_schedule_count);
  }
}

// Verifies that expired and non-expired items from all expirable states are
// properly handled.
TEST_F(StaleEntryFinalizerTaskTest, HandlesClockSetBackwardsCorrectly) {
  // Insert fresh and stale items for all expirable states from all buckets.
  PrefetchItem b1_item1_recent =
      CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, 23);
  PrefetchItem b1_item2_future =
      CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, 25);

  PrefetchItem b2_item1_recent =
      CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, 23);
  PrefetchItem b2_item2_future =
      CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, 25);
  PrefetchItem b2_item3_recent =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, 23);
  PrefetchItem b2_item4_future =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, 25);
  PrefetchItem b2_item5_recent =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, 23);
  PrefetchItem b2_item6_future =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, 25);

  PrefetchItem b3_item1_recent =
      CreateAndInsertItem(PrefetchItemState::DOWNLOADING, 23);
  PrefetchItem b3_item2_future =
      CreateAndInsertItem(PrefetchItemState::DOWNLOADING, 25);

  PrefetchItem b4_item1_future =
      CreateAndInsertItem(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE, 25);

  // Check inserted initial items.
  std::set<PrefetchItem> initial_items = {
      b1_item1_recent, b1_item2_future, b2_item1_recent, b2_item2_future,
      b2_item3_recent, b2_item4_future, b2_item5_recent, b2_item6_future,
      b3_item1_recent, b3_item2_future, b4_item1_future};
  std::set<PrefetchItem> all_inserted_items;
  EXPECT_EQ(11U, store_util()->GetAllItems(&all_inserted_items));
  EXPECT_EQ(initial_items, all_inserted_items);

  // Execute the expiration task.
  ExpectTaskCompletes(stale_finalizer_task_.get());
  stale_finalizer_task_->Run();
  RunUntilIdle();
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Create the expected finished version of each stale item.
  PrefetchItem b1_item2_finished(b1_item2_future);
  b1_item2_finished.state = PrefetchItemState::FINISHED;
  b1_item2_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b2_item2_finished(b2_item2_future);
  b2_item2_finished.state = PrefetchItemState::FINISHED;
  b2_item2_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b2_item4_finished(b2_item4_future);
  b2_item4_finished.state = PrefetchItemState::FINISHED;
  b2_item4_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b2_item6_finished(b2_item6_future);
  b2_item6_finished.state = PrefetchItemState::FINISHED;
  b2_item6_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b3_item2_finished(b3_item2_future);
  b3_item2_finished.state = PrefetchItemState::FINISHED;
  b3_item2_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b4_item_finished(b4_item1_future);
  b4_item1_future.state = PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE;
  b4_item1_future.error_code = PrefetchItemErrorCode::SUCCESS;

  // Creates the expected set of final items and compares with what's in
  // store.
  std::set<PrefetchItem> expected_final_items = {
      b1_item1_recent, b1_item2_finished, b2_item1_recent, b2_item2_finished,
      b2_item3_recent, b2_item4_finished, b2_item5_recent, b2_item6_finished,
      b3_item1_recent, b3_item2_finished, b4_item1_future};
  EXPECT_EQ(11U, expected_final_items.size());
  std::set<PrefetchItem> all_items_post_expiration;
  EXPECT_EQ(11U, store_util()->GetAllItems(&all_items_post_expiration));
  EXPECT_EQ(expected_final_items, all_items_post_expiration);
}

// Checks that items from all states are handled properly by the task when all
// their freshness dates are really old.
TEST_F(StaleEntryFinalizerTaskTest,
       HandleClockChangeBackwardsInAllStatesCorrectly) {
  // Insert "future" items for every state.
  const int many_hours = 7 * 24;
  CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, many_hours);
  CreateAndInsertItem(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE, many_hours);
  CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, many_hours);
  CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, many_hours);
  CreateAndInsertItem(PrefetchItemState::SENT_GET_OPERATION, many_hours);
  CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, many_hours);
  CreateAndInsertItem(PrefetchItemState::DOWNLOADING, many_hours);
  CreateAndInsertItem(PrefetchItemState::DOWNLOADED, many_hours);
  CreateAndInsertItem(PrefetchItemState::IMPORTING, many_hours);
  CreateAndInsertItem(PrefetchItemState::FINISHED, many_hours);
  CreateAndInsertItem(PrefetchItemState::ZOMBIE, many_hours);
  EXPECT_EQ(11, store_util()->CountPrefetchItems());

  // Execute the expiration task.
  ExpectTaskCompletes(stale_finalizer_task_.get());
  stale_finalizer_task_->Run();
  RunUntilIdle();
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Checks item counts for states expected to still exist.
  std::set<PrefetchItem> post_items;
  EXPECT_EQ(11U, store_util()->GetAllItems(&post_items));
  EXPECT_EQ(
      1U,
      Filter(post_items, PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE).size());
  EXPECT_EQ(1U,
            Filter(post_items, PrefetchItemState::SENT_GET_OPERATION).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::DOWNLOADED).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::IMPORTING).size());
  EXPECT_EQ(6U, Filter(post_items, PrefetchItemState::FINISHED).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::ZOMBIE).size());
}

}  // namespace offline_pages
