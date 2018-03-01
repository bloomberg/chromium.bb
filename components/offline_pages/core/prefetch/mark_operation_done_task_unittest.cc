// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/mark_operation_done_task.h"

#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/test_prefetch_gcm_handler.h"
#include "components/offline_pages/core/task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

const char kOperationName[] = "an_operation";
const char kOtherOperationName[] = "other_operation";

// All tests cases here only validate the request data and check for general
// http response. The tests for the Operation proto data returned in the http
// response are covered in PrefetchRequestOperationResponseTest.
class MarkOperationDoneTaskTest : public PrefetchTaskTestBase {
 public:
  MarkOperationDoneTaskTest() = default;
  ~MarkOperationDoneTaskTest() override = default;

  void SetUp() override { PrefetchTaskTestBase::SetUp(); }

  int64_t InsertAwaitingGCMOperation(std::string name) {
    return InsertPrefetchItemInStateWithOperation(
        name, PrefetchItemState::AWAITING_GCM);
  }

  int64_t InsertPrefetchItemInStateWithOperation(std::string operation_name,
                                                 PrefetchItemState state) {
    PrefetchItem item = item_generator()->CreateItem(state);
    item.operation_name = operation_name;
    EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
    return item.offline_id;
  }

  void ExpectStoreChangeCount(MarkOperationDoneTask* task,
                              int64_t change_count) {
    EXPECT_EQ(MarkOperationDoneTask::StoreResult::UPDATED,
              task->store_result());
    EXPECT_EQ(change_count, task->change_count());
    EXPECT_EQ(change_count > 0 ? 1 : 0, dispatcher()->task_schedule_count);
  }

  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }

 private:
  TestPrefetchDispatcher dispatcher_;
};

TEST_F(MarkOperationDoneTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  MarkOperationDoneTask task(dispatcher(), store(), kOperationName);
  ExpectTaskCompletes(&task);

  task.Run();
  RunUntilIdle();
}

TEST_F(MarkOperationDoneTaskTest, NoOpTask) {
  MarkOperationDoneTask task(dispatcher(), store(), kOperationName);
  ExpectTaskCompletes(&task);

  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, 0);
}

TEST_F(MarkOperationDoneTaskTest, SingleMatchingURL) {
  int64_t id = InsertAwaitingGCMOperation(kOperationName);
  MarkOperationDoneTask task(dispatcher(), store(), kOperationName);

  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, 1);

  EXPECT_EQ(1, store_util()->CountPrefetchItems());
  ASSERT_TRUE(store_util()->GetPrefetchItem(id));
  EXPECT_EQ(PrefetchItemState::RECEIVED_GCM,
            store_util()->GetPrefetchItem(id)->state);
}

TEST_F(MarkOperationDoneTaskTest, NoSuchURLs) {
  // Insert a record with an operation name.
  int64_t id1 = InsertAwaitingGCMOperation(kOperationName);

  // Start a task for an unrelated operation name.
  MarkOperationDoneTask task(dispatcher(), store(), kOtherOperationName);

  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, 0);

  ASSERT_TRUE(store_util()->GetPrefetchItem(id1));
  EXPECT_EQ(PrefetchItemState::AWAITING_GCM,
            store_util()->GetPrefetchItem(id1)->state);
}

TEST_F(MarkOperationDoneTaskTest, ManyURLs) {
  // Create 5 records with an operation name.
  std::vector<int64_t> ids;
  for (int i = 0; i < 5; i++) {
    ids.push_back(InsertAwaitingGCMOperation(kOperationName));
  }

  // Insert a record with a different operation name.
  int64_t id_other = InsertAwaitingGCMOperation(kOtherOperationName);

  ASSERT_EQ(6, store_util()->CountPrefetchItems());

  // Start a task for the first operation name.
  MarkOperationDoneTask task(dispatcher(), store(), kOperationName);

  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, ids.size());

  // The items should be in the new state.
  for (int64_t id : ids) {
    auto item = store_util()->GetPrefetchItem(id);
    ASSERT_TRUE(item);
    EXPECT_EQ(PrefetchItemState::RECEIVED_GCM, item->state);
  }

  // The other item should not be changed.
  ASSERT_TRUE(store_util()->GetPrefetchItem(id_other));
  EXPECT_EQ(PrefetchItemState::AWAITING_GCM,
            store_util()->GetPrefetchItem(id_other)->state);
}

TEST_F(MarkOperationDoneTaskTest, URLsInWrongState) {
  std::vector<int64_t> ids;
  ids.push_back(InsertPrefetchItemInStateWithOperation(
      kOperationName, PrefetchItemState::SENT_GET_OPERATION));
  ids.push_back(InsertPrefetchItemInStateWithOperation(
      kOperationName, PrefetchItemState::RECEIVED_BUNDLE));
  ids.push_back(InsertPrefetchItemInStateWithOperation(
      kOperationName, PrefetchItemState::DOWNLOADING));
  ids.push_back(InsertPrefetchItemInStateWithOperation(
      kOperationName, PrefetchItemState::FINISHED));
  ids.push_back(InsertPrefetchItemInStateWithOperation(
      kOperationName, PrefetchItemState::ZOMBIE));

  // Start a task for the first operation name.
  MarkOperationDoneTask task(dispatcher(), store(), kOperationName);

  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, 0);

  for (int64_t id : ids) {
    ASSERT_TRUE(store_util()->GetPrefetchItem(id));
    EXPECT_NE(PrefetchItemState::RECEIVED_GCM,
              store_util()->GetPrefetchItem(id)->state);
  }
}

}  // namespace offline_pages
