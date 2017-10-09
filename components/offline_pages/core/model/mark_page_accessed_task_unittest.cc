// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/mark_page_accessed_task.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const GURL kTestUrl("http://example.com");
const int64_t kTestOfflineId = 1234LL;
const char kTestClientNamespace[] = "default";
const ClientId kTestClientId(kTestClientNamespace, "1234");
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("/test/path/file"));
const int64_t kTestFileSize = 876543LL;
}  // namespace

class MarkPageAccessedTaskTest : public testing::Test {
 public:
  MarkPageAccessedTaskTest();
  ~MarkPageAccessedTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  TestTaskRunner* runner() { return &runner_; }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  TestTaskRunner runner_;
};

MarkPageAccessedTaskTest::MarkPageAccessedTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_) {}

MarkPageAccessedTaskTest::~MarkPageAccessedTaskTest() {}

void MarkPageAccessedTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
}

void MarkPageAccessedTaskTest::TearDown() {
  store_test_util_.DeleteStore();
}

TEST_F(MarkPageAccessedTaskTest, MarkPageAccessed) {
  OfflinePageItem page(kTestUrl, kTestOfflineId, kTestClientId, kTestFilePath,
                       kTestFileSize);
  store_test_util()->InsertItem(page);

  base::Time current_time = base::Time::Now();
  auto task = base::MakeUnique<MarkPageAccessedTask>(store(), kTestOfflineId,
                                                     current_time);
  runner()->RunTask(std::move(task));

  auto offline_page = store_test_util()->GetPageByOfflineId(kTestOfflineId);
  EXPECT_EQ(kTestUrl, offline_page->url);
  EXPECT_EQ(kTestClientId, offline_page->client_id);
  EXPECT_EQ(kTestFileSize, offline_page->file_size);
  EXPECT_EQ(1, offline_page->access_count);
  EXPECT_EQ(current_time, offline_page->last_access_time);
}

TEST_F(MarkPageAccessedTaskTest, MarkPageAccessedTwice) {
  OfflinePageItem page(kTestUrl, kTestOfflineId, kTestClientId, kTestFilePath,
                       kTestFileSize);
  store_test_util()->InsertItem(page);

  base::Time current_time = base::Time::Now();
  auto task = base::MakeUnique<MarkPageAccessedTask>(store(), kTestOfflineId,
                                                     current_time);
  runner()->RunTask(std::move(task));

  auto offline_page = store_test_util()->GetPageByOfflineId(kTestOfflineId);
  EXPECT_EQ(kTestOfflineId, offline_page->offline_id);
  EXPECT_EQ(kTestUrl, offline_page->url);
  EXPECT_EQ(kTestClientId, offline_page->client_id);
  EXPECT_EQ(kTestFileSize, offline_page->file_size);
  EXPECT_EQ(1, offline_page->access_count);
  EXPECT_EQ(current_time, offline_page->last_access_time);

  task = base::MakeUnique<MarkPageAccessedTask>(store(), kTestOfflineId,
                                                base::Time::Now());
  runner()->RunTask(std::move(task));

  offline_page = store_test_util()->GetPageByOfflineId(kTestOfflineId);
  EXPECT_EQ(kTestOfflineId, offline_page->offline_id);
  EXPECT_EQ(2, offline_page->access_count);
  EXPECT_LT(current_time, offline_page->last_access_time);
}

}  // namespace offline_pages
