// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/persistent_pages_consistency_check_task.h"

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

class PersistentPagesConsistencyCheckTaskTest : public testing::Test {
 public:
  PersistentPagesConsistencyCheckTaskTest();
  ~PersistentPagesConsistencyCheckTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  OfflinePageItem AddPage(const std::string& name_space,
                          const base::FilePath& archive_dir);
  void SetShouldCreateDbEntry(bool should_create_db_entry);
  void SetShouldCreateFile(bool should_create_file);
  bool IsPageRemovedFromBothPlaces(const OfflinePageItem& page);

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TestTaskRunner* runner() { return &runner_; }
  ClientPolicyController* policy_controller() {
    return policy_controller_.get();
  }

  const base::FilePath& persistent_dir() { return persistent_dir_.GetPath(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;
  std::unique_ptr<ClientPolicyController> policy_controller_;

  base::ScopedTempDir persistent_dir_;
  bool should_create_db_entry_;
  bool should_create_file_;
};

PersistentPagesConsistencyCheckTaskTest::
    PersistentPagesConsistencyCheckTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_),
      should_create_db_entry_(false),
      should_create_file_(false) {}

PersistentPagesConsistencyCheckTaskTest::
    ~PersistentPagesConsistencyCheckTaskTest() {}

void PersistentPagesConsistencyCheckTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
  ASSERT_TRUE(persistent_dir_.CreateUniqueTempDir());
  policy_controller_ = base::MakeUnique<ClientPolicyController>();
}

void PersistentPagesConsistencyCheckTaskTest::TearDown() {
  store_test_util_.DeleteStore();
  if (!persistent_dir_.Delete())
    DVLOG(1) << "ScopedTempDir deletion failed.";
  task_runner_->RunUntilIdle();
}

OfflinePageItem PersistentPagesConsistencyCheckTaskTest::AddPage(
    const std::string& name_space,
    const base::FilePath& archive_dir) {
  generator()->SetNamespace(name_space);
  generator()->SetArchiveDirectory(archive_dir);
  OfflinePageItem page = generator()->CreateItemWithTempFile();
  if (should_create_db_entry_)
    store_test_util()->InsertItem(page);
  if (!should_create_file_)
    EXPECT_TRUE(base::DeleteFile(page.file_path, false));
  return page;
}

void PersistentPagesConsistencyCheckTaskTest::SetShouldCreateDbEntry(
    bool should_create_db_entry) {
  should_create_db_entry_ = should_create_db_entry;
}

void PersistentPagesConsistencyCheckTaskTest::SetShouldCreateFile(
    bool should_create_file) {
  should_create_file_ = should_create_file;
}

bool PersistentPagesConsistencyCheckTaskTest::IsPageRemovedFromBothPlaces(
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
TEST_F(PersistentPagesConsistencyCheckTaskTest,
       MAYBE_TestDeleteFileWithoutDbEntry) {
  // Only the files without DB entry and in persistent archive directory will
  // be deleted.
  SetShouldCreateFile(true);

  SetShouldCreateDbEntry(true);
  OfflinePageItem page1 = AddPage(kDownloadNamespace, persistent_dir());
  SetShouldCreateDbEntry(false);
  OfflinePageItem page2 = AddPage(kDownloadNamespace, persistent_dir());

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2LL, GetFileCountInDir(persistent_dir()));

  auto task = base::MakeUnique<PersistentPagesConsistencyCheckTask>(
      store(), policy_controller(), persistent_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1LL, GetFileCountInDir(persistent_dir()));
  EXPECT_FALSE(IsPageRemovedFromBothPlaces(page1));
  EXPECT_TRUE(IsPageRemovedFromBothPlaces(page2));
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeleteDbEntryWithoutFile DISABLED_TestDeleteDbEntryWithoutFile
#else
#define MAYBE_TestDeleteDbEntryWithoutFile TestDeleteDbEntryWithoutFile
#endif
TEST_F(PersistentPagesConsistencyCheckTaskTest,
       MAYBE_TestDeleteDbEntryWithoutFile) {
  // The persistent pages will be deleted from DB if their DB entries exist but
  // files are missing.
  SetShouldCreateDbEntry(true);

  SetShouldCreateFile(true);
  OfflinePageItem page1 = AddPage(kDownloadNamespace, persistent_dir());
  SetShouldCreateFile(false);
  OfflinePageItem page2 = AddPage(kDownloadNamespace, persistent_dir());

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1LL, GetFileCountInDir(persistent_dir()));

  auto task = base::MakeUnique<PersistentPagesConsistencyCheckTask>(
      store(), policy_controller(), persistent_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1LL, GetFileCountInDir(persistent_dir()));
  EXPECT_FALSE(IsPageRemovedFromBothPlaces(page1));
  EXPECT_TRUE(IsPageRemovedFromBothPlaces(page2));
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_CombinedTest DISABLED_CombinedTest
#else
#define MAYBE_CombinedTest CombinedTest
#endif
TEST_F(PersistentPagesConsistencyCheckTaskTest, MAYBE_CombinedTest) {
  // Adding a bunch of pages with different setups.
  // After the consistency check, only page1 will exist.
  SetShouldCreateDbEntry(true);
  SetShouldCreateFile(true);
  OfflinePageItem page1 = AddPage(kDownloadNamespace, persistent_dir());
  SetShouldCreateDbEntry(false);
  SetShouldCreateFile(true);
  OfflinePageItem page2 = AddPage(kDownloadNamespace, persistent_dir());
  SetShouldCreateDbEntry(true);
  SetShouldCreateFile(false);
  OfflinePageItem page3 = AddPage(kDownloadNamespace, persistent_dir());

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2LL, GetFileCountInDir(persistent_dir()));

  auto task = base::MakeUnique<PersistentPagesConsistencyCheckTask>(
      store(), policy_controller(), persistent_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1LL, GetFileCountInDir(persistent_dir()));
  EXPECT_FALSE(IsPageRemovedFromBothPlaces(page1));
  EXPECT_TRUE(IsPageRemovedFromBothPlaces(page2));
  EXPECT_TRUE(IsPageRemovedFromBothPlaces(page3));
}

TEST_F(PersistentPagesConsistencyCheckTaskTest, TestKeepingNonMhtmlFile) {
  // The persistent pages will be deleted from DB if their DB entries exist but
  // files are missing.
  SetShouldCreateDbEntry(false);
  SetShouldCreateFile(true);
  OfflinePageItem page1 = AddPage(kDownloadNamespace, persistent_dir());
  // Create a file with non-mhtml extension.
  base::FilePath path;
  base::CreateTemporaryFileInDir(persistent_dir(), &path);
  base::FilePath mp3_path = path.AddExtension(FILE_PATH_LITERAL("mp3"));
  EXPECT_TRUE(base::Move(path, mp3_path));

  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2LL, GetFileCountInDir(persistent_dir()));

  auto task = base::MakeUnique<PersistentPagesConsistencyCheckTask>(
      store(), policy_controller(), persistent_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1LL, GetFileCountInDir(persistent_dir()));
  EXPECT_TRUE(IsPageRemovedFromBothPlaces(page1));
}

}  // namespace offline_pages
