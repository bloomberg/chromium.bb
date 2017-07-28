// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/mark_operation_done_task.h"

#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_gcm_handler.h"
#include "components/offline_pages/core/task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::HasSubstr;
using testing::DoAll;
using testing::SaveArg;

namespace offline_pages {

const int kStoreFailure = PrefetchStoreTestUtil::kStoreCommandFailed;
const char kOperationName[] = "an_operation";
const char kOtherOperationName[] = "other_operation";

// All tests cases here only validate the request data and check for general
// http response. The tests for the Operation proto data returned in the http
// response are covered in PrefetchRequestOperationResponseTest.
class MarkOperationDoneTaskTest : public TaskTestBase {
 public:
  MarkOperationDoneTaskTest() = default;
  ~MarkOperationDoneTaskTest() override = default;

  void SetUp() override { TaskTestBase::SetUp(); }

  int64_t InsertAwaitingGCMOperation(std::string name) {
    return InsertPrefetchItemInStateWithOperation(
        name, PrefetchItemState::AWAITING_GCM);
  }

  int64_t InsertPrefetchItemInStateWithOperation(std::string operation_name,
                                                 PrefetchItemState state) {
    PrefetchItem item;
    item.state = state;
    item.offline_id = PrefetchStoreUtils::GenerateOfflineId();
    std::string offline_id_string = std::to_string(item.offline_id);
    item.url = GURL("http://www.example.com/?id=" + offline_id_string);
    item.operation_name = operation_name;
    int64_t id = store_util()->InsertPrefetchItem(item);
    EXPECT_NE(kStoreFailure, id);
    return id;
  }

  void ExpectStoreChangeCount(MarkOperationDoneTask* task,
                              int64_t change_count) {
    EXPECT_EQ(MarkOperationDoneTask::StoreResult::UPDATED,
              task->store_result());
    EXPECT_EQ(change_count, task->change_count());
  }

  PrefetchDispatcher* dispatcher() { return nullptr; }
};

TEST_F(MarkOperationDoneTaskTest, NoOpTask) {
  MarkOperationDoneTask task(store(), kOperationName);
  ExpectTaskCompletes(&task);

  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, 0);
}

TEST_F(MarkOperationDoneTaskTest, SingleMatchingURL) {
  int64_t id = InsertAwaitingGCMOperation(kOperationName);
  MarkOperationDoneTask task(store(), kOperationName);

  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, 1);

  EXPECT_EQ(1, store_util()->CountPrefetchItems());
  EXPECT_TRUE(store_util()->GetPrefetchItem(id));
  EXPECT_EQ(PrefetchItemState::RECEIVED_GCM,
            store_util()->GetPrefetchItem(id)->state);
}

TEST_F(MarkOperationDoneTaskTest, NoSuchURLs) {
  // Insert a record with an operation name.
  int64_t id1 = InsertAwaitingGCMOperation(kOperationName);

  // Start a task for an unrelated operation name.
  MarkOperationDoneTask task(store(), kOtherOperationName);

  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, 0);

  EXPECT_TRUE(store_util()->GetPrefetchItem(id1));
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
  MarkOperationDoneTask task(store(), kOperationName);

  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, ids.size());

  // The items should be in the new state.
  for (int64_t id : ids) {
    auto item = store_util()->GetPrefetchItem(id);
    EXPECT_TRUE(item);
    EXPECT_EQ(PrefetchItemState::RECEIVED_GCM, item->state);
  }

  // The other item should not be changed.
  EXPECT_TRUE(store_util()->GetPrefetchItem(id_other));
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
  MarkOperationDoneTask task(store(), kOperationName);

  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
  ExpectStoreChangeCount(&task, 0);

  for (int64_t id : ids) {
    EXPECT_TRUE(store_util()->GetPrefetchItem(id));
    EXPECT_NE(PrefetchItemState::RECEIVED_GCM,
              store_util()->GetPrefetchItem(id)->state);
  }
}

}  // namespace offline_pages
