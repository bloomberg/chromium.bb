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
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

int64_t GetFileCountInDir(const base::FilePath& dir) {
  base::FileEnumerator file_enumerator(dir, false, base::FileEnumerator::FILES);
  int64_t count = 0;
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    count++;
  }
  return count;
}

}  // namespace

class TemporaryPagesConsistencyCheckTaskTest : public testing::Test {
 public:
  TemporaryPagesConsistencyCheckTaskTest();
  ~TemporaryPagesConsistencyCheckTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  OfflinePageItem AddPage(const std::string& name_space,
                          const base::FilePath& archive_dir);
  void SetRemoveDbEntryAndFile(bool remove_db_entry, bool remove_file);

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TestTaskRunner* runner() { return &runner_; }
  ClientPolicyController* policy_controller() {
    return policy_controller_.get();
  }

  const base::FilePath& legacy_archives_dir() {
    return legacy_archives_dir_.GetPath();
  }
  const base::FilePath& temporary_dir() { return temporary_dir_.GetPath(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;
  std::unique_ptr<ClientPolicyController> policy_controller_;

  base::ScopedTempDir legacy_archives_dir_;
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
  ASSERT_TRUE(legacy_archives_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  policy_controller_ = base::MakeUnique<ClientPolicyController>();
}

void TemporaryPagesConsistencyCheckTaskTest::TearDown() {
  store_test_util_.DeleteStore();
  if (!legacy_archives_dir_.Delete())
    DVLOG(1) << "ScopedTempDir deletion failed.";
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

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeletePageInLegacyArchivesDir \
  DISABLED_TestDeletePageInLegacyArchivesDir
#else
#define MAYBE_TestDeletePageInLegacyArchivesDir \
  TestDeletePageInLegacyArchivesDir
#endif
TEST_F(TemporaryPagesConsistencyCheckTaskTest,
       MAYBE_TestDeletePageInLegacyArchivesDir) {
  // The temporary pages in legacy archives directory will be deleted from both
  // DB and disk.
  SetRemoveDbEntryAndFile(false, false);
  OfflinePageItem page1 = AddPage(kLastNNamespace, legacy_archives_dir());
  OfflinePageItem page2 = AddPage(kDownloadNamespace, legacy_archives_dir());
  OfflinePageItem page3 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2LL, GetFileCountInDir(legacy_archives_dir()));

  auto task = base::MakeUnique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir(), legacy_archives_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1LL, GetFileCountInDir(legacy_archives_dir()));
  EXPECT_EQ(1LL, GetFileCountInDir(temporary_dir()));
  EXPECT_FALSE(store_test_util()->GetPageByOfflineId(page1.offline_id));
}

TEST_F(TemporaryPagesConsistencyCheckTaskTest, TestDeleteFileWithoutDbEntry) {
  // Since the files in legacy archives directory are missing DB entries,
  // there's no way to know if the pages were temporary or persistent. So
  // they'll not be removed, only the file without DB entry and in temporary
  // directory will be deleted.
  SetRemoveDbEntryAndFile(true, false);
  OfflinePageItem page1 = AddPage(kLastNNamespace, legacy_archives_dir());
  OfflinePageItem page2 = AddPage(kDownloadNamespace, legacy_archives_dir());
  OfflinePageItem page3 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2LL, GetFileCountInDir(legacy_archives_dir()));
  EXPECT_EQ(1LL, GetFileCountInDir(temporary_dir()));

  auto task = base::MakeUnique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir(), legacy_archives_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2LL, GetFileCountInDir(legacy_archives_dir()));
  EXPECT_EQ(0LL, GetFileCountInDir(temporary_dir()));
  EXPECT_TRUE(base::PathExists(page2.file_path));
}

TEST_F(TemporaryPagesConsistencyCheckTaskTest, TestDeleteDBEntryWithoutFile) {
  // The temporary pages will be deleted from DB if their DB entries exist but
  // files are missing.
  // In this test case, page1 and page3 will be deleted from DB. page2 will be
  // left behind in DB since it's a persistent page.
  SetRemoveDbEntryAndFile(false, true);
  OfflinePageItem page1 = AddPage(kLastNNamespace, legacy_archives_dir());
  OfflinePageItem page2 = AddPage(kDownloadNamespace, legacy_archives_dir());
  OfflinePageItem page3 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_EQ(0LL, GetFileCountInDir(legacy_archives_dir()));
  EXPECT_EQ(0LL, GetFileCountInDir(temporary_dir()));

  auto task = base::MakeUnique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir(), legacy_archives_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(0LL, GetFileCountInDir(legacy_archives_dir()));
  EXPECT_EQ(0LL, GetFileCountInDir(temporary_dir()));
  EXPECT_FALSE(store_test_util()->GetPageByOfflineId(page1.offline_id));
  EXPECT_EQ(page2.file_path,
            store_test_util()->GetPageByOfflineId(page2.offline_id)->file_path);
  EXPECT_FALSE(store_test_util()->GetPageByOfflineId(page3.offline_id));
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
  // After the consistency check, DB will contain page2, page3, page8.
  // The files of page2, page3, page4, page5, page9 will still exist.
  SetRemoveDbEntryAndFile(false, false);
  OfflinePageItem page1 = AddPage(kLastNNamespace, legacy_archives_dir());
  OfflinePageItem page2 = AddPage(kDownloadNamespace, legacy_archives_dir());
  OfflinePageItem page3 = AddPage(kLastNNamespace, temporary_dir());
  SetRemoveDbEntryAndFile(true, false);
  OfflinePageItem page4 = AddPage(kLastNNamespace, legacy_archives_dir());
  OfflinePageItem page5 = AddPage(kDownloadNamespace, legacy_archives_dir());
  OfflinePageItem page6 = AddPage(kLastNNamespace, temporary_dir());
  SetRemoveDbEntryAndFile(false, true);
  OfflinePageItem page7 = AddPage(kLastNNamespace, legacy_archives_dir());
  OfflinePageItem page8 = AddPage(kDownloadNamespace, legacy_archives_dir());
  OfflinePageItem page9 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(6LL, store_test_util()->GetPageCount());
  EXPECT_EQ(4LL, GetFileCountInDir(legacy_archives_dir()));
  EXPECT_EQ(2LL, GetFileCountInDir(temporary_dir()));

  auto task = base::MakeUnique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir(), legacy_archives_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_EQ(3LL, GetFileCountInDir(legacy_archives_dir()));
  EXPECT_EQ(1LL, GetFileCountInDir(temporary_dir()));
}

}  // namespace offline_pages
