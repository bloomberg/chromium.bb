// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/temporary_pages_consistency_check_task.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/model/offline_page_test_util.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class TemporaryPagesConsistencyCheckTaskTest : public testing::Test {
 public:
  TemporaryPagesConsistencyCheckTaskTest();
  ~TemporaryPagesConsistencyCheckTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  OfflinePageItem AddPage(const std::string& name_space,
                          const base::FilePath& archive_dir);
  void SetRemoveDbEntryAndFile(bool remove_db_entry, bool remove_file);
  bool IsPageRemoved(const OfflinePageItem& page);

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TestTaskRunner* runner() { return &runner_; }
  ClientPolicyController* policy_controller() {
    return policy_controller_.get();
  }
  const base::FilePath& temporary_dir() { return temporary_dir_.GetPath(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;
  std::unique_ptr<ClientPolicyController> policy_controller_;

  base::ScopedTempDir temporary_dir_;
  bool remove_db_entry_;
  bool remove_file_;
};

TemporaryPagesConsistencyCheckTaskTest::TemporaryPagesConsistencyCheckTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_),
      remove_db_entry_(false),
      remove_file_(false) {}

TemporaryPagesConsistencyCheckTaskTest::
    ~TemporaryPagesConsistencyCheckTaskTest() {}

void TemporaryPagesConsistencyCheckTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  policy_controller_ = base::MakeUnique<ClientPolicyController>();
}

void TemporaryPagesConsistencyCheckTaskTest::TearDown() {
  store_test_util_.DeleteStore();
  if (!temporary_dir_.Delete())
    DVLOG(1) << "ScopedTempDir deletion failed.";
  task_runner_->RunUntilIdle();
}

OfflinePageItem TemporaryPagesConsistencyCheckTaskTest::AddPage(
    const std::string& name_space,
    const base::FilePath& archive_dir) {
  generator()->SetNamespace(name_space);
  generator()->SetArchiveDirectory(archive_dir);
  OfflinePageItem page = generator()->CreateItemWithTempFile();
  if (!remove_db_entry_)
    store_test_util()->InsertItem(page);
  if (remove_file_)
    EXPECT_TRUE(base::DeleteFile(page.file_path, false));
  return page;
}

void TemporaryPagesConsistencyCheckTaskTest::SetRemoveDbEntryAndFile(
    bool remove_db_entry,
    bool remove_file) {
  remove_db_entry_ = remove_db_entry;
  remove_file_ = remove_file;
}

bool TemporaryPagesConsistencyCheckTaskTest::IsPageRemoved(
    const OfflinePageItem& page) {
  return !base::PathExists(page.file_path) &&
         !store_test_util()->GetPageByOfflineId(page.offline_id);
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeleteFileWithoutDbEntry DISABLED_TestDeleteFileWithoutDbEntry
#else
#define MAYBE_TestDeleteFileWithoutDbEntry TestDeleteFileWithoutDbEntry
#endif
TEST_F(TemporaryPagesConsistencyCheckTaskTest,
       MAYBE_TestDeleteFileWithoutDbEntry) {
  // Only the file without DB entry and in temporary directory will be deleted.
  SetRemoveDbEntryAndFile(false, false);
  OfflinePageItem page1 = AddPage(kLastNNamespace, temporary_dir());
  SetRemoveDbEntryAndFile(true, false);
  OfflinePageItem page2 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_util::GetFileCountInDirectory(temporary_dir()));

  auto task = base::MakeUnique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir()));
  EXPECT_FALSE(IsPageRemoved(page1));
  EXPECT_TRUE(IsPageRemoved(page2));
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeleteDbEntryWithoutFile DISABLED_TestDeleteDbEntryWithoutFile
#else
#define MAYBE_TestDeleteDbEntryWithoutFile TestDeleteDbEntryWithoutFile
#endif
TEST_F(TemporaryPagesConsistencyCheckTaskTest,
       MAYBE_TestDeleteDbEntryWithoutFile) {
  // The temporary pages will be deleted from DB if their DB entries exist but
  // files are missing.
  SetRemoveDbEntryAndFile(false, false);
  OfflinePageItem page1 = AddPage(kLastNNamespace, temporary_dir());
  SetRemoveDbEntryAndFile(false, true);
  OfflinePageItem page2 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir()));

  auto task = base::MakeUnique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir()));
  EXPECT_FALSE(IsPageRemoved(page1));
  EXPECT_TRUE(IsPageRemoved(page2));
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_CombinedTest DISABLED_CombinedTest
#else
#define MAYBE_CombinedTest CombinedTest
#endif
TEST_F(TemporaryPagesConsistencyCheckTaskTest, MAYBE_CombinedTest) {
  // Adding a bunch of pages with different setups.
  // After the consistency check, only page1 will exist.
  SetRemoveDbEntryAndFile(false, false);
  OfflinePageItem page1 = AddPage(kLastNNamespace, temporary_dir());
  SetRemoveDbEntryAndFile(true, false);
  OfflinePageItem page2 = AddPage(kLastNNamespace, temporary_dir());
  SetRemoveDbEntryAndFile(false, true);
  OfflinePageItem page3 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_util::GetFileCountInDirectory(temporary_dir()));

  auto task = base::MakeUnique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir()));
  EXPECT_FALSE(IsPageRemoved(page1));
  EXPECT_TRUE(IsPageRemoved(page2));
  EXPECT_TRUE(IsPageRemoved(page3));
}

}  // namespace offline_pages
