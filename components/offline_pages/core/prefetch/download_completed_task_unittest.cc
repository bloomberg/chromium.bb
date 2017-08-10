// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/download_completed_task.h"

#include <string>
#include <vector>

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const int64_t kTestOfflineID = 1111;
const int64_t kTestOfflineID2 = 223344;
const char kTestGUID[] = "1a150628-1b56-44da-a85a-c575120af180";
const char kTestGUID2[] = "736edb12-98f6-41c2-8e50-a667694511a5";
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("foo"));
const int64_t kTestFileSize = 88888;
}  // namespace

class DownloadCompletedTaskTest : public testing::Test {
 public:
  DownloadCompletedTaskTest();
  ~DownloadCompletedTaskTest() override = default;

  void SetUp() override;
  void TearDown() override;

  void PumpLoop();

  PrefetchStore* store() { return store_test_util_.store(); }
  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }
  PrefetchStoreTestUtil* store_util() { return &store_test_util_; }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  TestPrefetchDispatcher dispatcher_;
  PrefetchStoreTestUtil store_test_util_;
};

DownloadCompletedTaskTest::DownloadCompletedTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_) {}

void DownloadCompletedTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();

  PrefetchItem item;
  item.offline_id = kTestOfflineID;
  item.guid = kTestGUID;
  item.state = PrefetchItemState::DOWNLOADING;
  item.creation_time = base::Time::Now();
  item.freshness_time = item.creation_time;
  EXPECT_TRUE(store_test_util_.InsertPrefetchItem(item));

  PrefetchItem item2;
  item2.offline_id = kTestOfflineID2;
  item2.guid = kTestGUID2;
  item2.state = PrefetchItemState::NEW_REQUEST;
  item2.creation_time = base::Time::Now();
  item2.freshness_time = item.creation_time;
  EXPECT_TRUE(store_test_util_.InsertPrefetchItem(item2));
}

void DownloadCompletedTaskTest::TearDown() {
  store_test_util_.DeleteStore();
  PumpLoop();
}

void DownloadCompletedTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

TEST_F(DownloadCompletedTaskTest, UpdateItemOnDownloadSuccess) {
  PrefetchDownloadResult download_result(kTestGUID, kTestFilePath,
                                         kTestFileSize);
  DownloadCompletedTask task(dispatcher(), store(), download_result);
  task.Run();
  PumpLoop();

  std::unique_ptr<PrefetchItem> item =
      store_util()->GetPrefetchItem(kTestOfflineID);
  EXPECT_EQ(PrefetchItemState::DOWNLOADED, item->state);
  EXPECT_EQ(kTestGUID, item->guid);
  EXPECT_EQ(kTestFilePath, item->file_path);
  EXPECT_EQ(kTestFileSize, item->file_size);
}

TEST_F(DownloadCompletedTaskTest, UpdateItemOnDownloadError) {
  PrefetchDownloadResult download_result;
  download_result.download_id = kTestGUID;
  download_result.success = false;
  DownloadCompletedTask task(dispatcher(), store(), download_result);
  task.Run();
  PumpLoop();

  std::unique_ptr<PrefetchItem> item =
      store_util()->GetPrefetchItem(kTestOfflineID);
  EXPECT_EQ(PrefetchItemState::FINISHED, item->state);
  EXPECT_EQ(PrefetchItemErrorCode::DOWNLOAD_ERROR, item->error_code);
  EXPECT_EQ(kTestGUID, item->guid);
  EXPECT_TRUE(item->file_path.empty());
  EXPECT_EQ(0, item->file_size);
}

TEST_F(DownloadCompletedTaskTest, NoUpdateOnMismatchedDownloadSuccess) {
  PrefetchDownloadResult download_result(kTestGUID2, kTestFilePath,
                                         kTestFileSize);
  DownloadCompletedTask task(dispatcher(), store(), download_result);
  task.Run();
  PumpLoop();

  // Item will only be updated when both offline_id and state match.
  std::unique_ptr<PrefetchItem> item =
      store_util()->GetPrefetchItem(kTestOfflineID);
  EXPECT_EQ(PrefetchItemState::DOWNLOADING, item->state);

  std::unique_ptr<PrefetchItem> item2 =
      store_util()->GetPrefetchItem(kTestOfflineID2);
  EXPECT_EQ(PrefetchItemState::NEW_REQUEST, item2->state);
}

TEST_F(DownloadCompletedTaskTest, NoUpdateOnMismatchedDownloadError) {
  PrefetchDownloadResult download_result;
  download_result.download_id = kTestGUID2;
  download_result.success = false;
  DownloadCompletedTask task(dispatcher(), store(), download_result);
  task.Run();
  PumpLoop();

  // Item will only be updated when both offline_id and state match.
  std::unique_ptr<PrefetchItem> item =
      store_util()->GetPrefetchItem(kTestOfflineID);
  EXPECT_EQ(PrefetchItemState::DOWNLOADING, item->state);

  std::unique_ptr<PrefetchItem> item2 =
      store_util()->GetPrefetchItem(kTestOfflineID2);
  EXPECT_EQ(PrefetchItemState::NEW_REQUEST, item2->state);
}

}  // namespace offline_pages
