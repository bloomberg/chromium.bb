// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/add_page_to_download_manager_task.h"

#include "base/memory/ptr_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/system_download_manager_stub.h"
#include "components/offline_pages/core/task.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTitle[] = "testPageTitle";
const char kDescription[] = "test page description";
const char kPath[] = "/sdcard/Download/page.mhtml";
const char kUri[] = "https://www.google.com";
const char kReferer[] = "https://google.com";
const long kTestLength = 1024;
const long kTestDownloadId = 42;
const long kDefaultDownloadId = 1;
}  // namespace

namespace offline_pages {

class AddPageToDownloadManagerTaskTest : public testing::Test {
 public:
  AddPageToDownloadManagerTaskTest();
  ~AddPageToDownloadManagerTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }

  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }

  OfflinePageItemGenerator* generator() { return &generator_; }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner() {
    return task_runner_.get();
  }

  SystemDownloadManagerStub* download_manager() {
    return download_manager_.get();
  }

  void AddTaskDone(Task* task) { add_task_done_ = true; }

  bool add_task_done() { return add_task_done_; }

  void PumpLoop();

  void SetTaskCompletionCallbackForTesting(Task* task);

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  std::unique_ptr<SystemDownloadManagerStub> download_manager_;
  bool add_task_done_;
};

AddPageToDownloadManagerTaskTest::AddPageToDownloadManagerTaskTest()
    : task_runner_(new base::TestMockTimeTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      download_manager_(new SystemDownloadManagerStub(kTestDownloadId, true)),
      add_task_done_(false) {}

AddPageToDownloadManagerTaskTest::~AddPageToDownloadManagerTaskTest() {}

void AddPageToDownloadManagerTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
}

void AddPageToDownloadManagerTaskTest::TearDown() {
  store_test_util_.DeleteStore();
}

void AddPageToDownloadManagerTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void AddPageToDownloadManagerTaskTest::SetTaskCompletionCallbackForTesting(
    Task* task) {
  task->SetTaskCompletionCallbackForTesting(
      task_runner_.get(),
      base::BindRepeating(&AddPageToDownloadManagerTaskTest::AddTaskDone,
                          base::Unretained(this)));
}

TEST_F(AddPageToDownloadManagerTaskTest, AddSimpleId) {
  OfflinePageItem page = generator()->CreateItem();
  store_test_util()->InsertItem(page);

  auto task = base::MakeUnique<AddPageToDownloadManagerTask>(
      store(), download_manager(), page.offline_id, kTitle, kDescription, kPath,
      kTestLength, kUri, kReferer);
  SetTaskCompletionCallbackForTesting(task.get());
  task->Run();
  PumpLoop();

  // Check that task finished running.
  ASSERT_TRUE(add_task_done());

  // Check the download ID got set in the offline page item in the database.
  auto offline_page = store_test_util()->GetPageByOfflineId(page.offline_id);
  ASSERT_TRUE(offline_page);
  EXPECT_EQ(offline_page->system_download_id, kTestDownloadId);

  // Check that the system download manager stub saw the arguments it expected
  EXPECT_EQ(download_manager()->title(), std::string(kTitle));
  EXPECT_EQ(download_manager()->description(), kDescription);
  EXPECT_EQ(download_manager()->path(), kPath);
  EXPECT_EQ(download_manager()->uri(), kUri);
  EXPECT_EQ(download_manager()->referer(), kReferer);
  EXPECT_EQ(download_manager()->length(), kTestLength);
}

TEST_F(AddPageToDownloadManagerTaskTest, NoADM) {
  // Simulate the ADM being unavailable on the system.
  download_manager()->set_installed(false);

  OfflinePageItem page = generator()->CreateItem();
  store_test_util()->InsertItem(page);

  auto task = base::MakeUnique<AddPageToDownloadManagerTask>(
      store(), download_manager(), page.offline_id, kTitle, kDescription, kPath,
      kTestLength, kUri, kReferer);
  SetTaskCompletionCallbackForTesting(task.get());
  task->Run();
  PumpLoop();

  // Check that task finished running.
  ASSERT_TRUE(add_task_done());

  // Check the download ID did not get set in the offline page item in the
  // database.
  auto offline_page = store_test_util()->GetPageByOfflineId(page.offline_id);
  ASSERT_TRUE(offline_page);
  EXPECT_EQ(offline_page->system_download_id, 0);
}

TEST_F(AddPageToDownloadManagerTaskTest, AddDownloadFailed) {
  // Simulate failure by asking the download manager to return id of 0.
  download_manager()->set_download_id(0);
  OfflinePageItem page = generator()->CreateItem();
  page.system_download_id = kDefaultDownloadId;
  store_test_util()->InsertItem(page);

  auto task = base::MakeUnique<AddPageToDownloadManagerTask>(
      store(), download_manager(), page.offline_id, kTitle, kDescription, kPath,
      kTestLength, kUri, kReferer);
  SetTaskCompletionCallbackForTesting(task.get());
  task->Run();
  PumpLoop();

  // Check that task finished running.
  ASSERT_TRUE(add_task_done());

  // Check the download ID did not get set in the offline page item in the
  // database.
  auto offline_page = store_test_util()->GetPageByOfflineId(page.offline_id);
  ASSERT_TRUE(offline_page);
  EXPECT_EQ(offline_page->system_download_id, kDefaultDownloadId);
}

}  // namespace offline_pages
