// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/temporary_pages_consistency_check_task.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/model/offline_page_test_utils.h"
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
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  const base::FilePath& temporary_dir() { return temporary_dir_.GetPath(); }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;
  std::unique_ptr<ClientPolicyController> policy_controller_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  base::ScopedTempDir temporary_dir_;
  bool should_create_db_entry_;
  bool should_create_file_;
};

TemporaryPagesConsistencyCheckTaskTest::TemporaryPagesConsistencyCheckTaskTest()
    : task_runner_(new base::TestMockTimeTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_),
      should_create_db_entry_(false),
      should_create_file_(false) {}

TemporaryPagesConsistencyCheckTaskTest::
    ~TemporaryPagesConsistencyCheckTaskTest() {}

void TemporaryPagesConsistencyCheckTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  policy_controller_ = std::make_unique<ClientPolicyController>();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
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
  if (should_create_db_entry_)
    store_test_util()->InsertItem(page);
  if (!should_create_file_)
    EXPECT_TRUE(base::DeleteFile(page.file_path, false));
  return page;
}

void TemporaryPagesConsistencyCheckTaskTest::SetShouldCreateDbEntry(
    bool should_create_db_entry) {
  should_create_db_entry_ = should_create_db_entry;
}

void TemporaryPagesConsistencyCheckTaskTest::SetShouldCreateFile(
    bool should_create_file) {
  should_create_file_ = should_create_file;
}

bool TemporaryPagesConsistencyCheckTaskTest::IsPageRemovedFromBothPlaces(
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
  SetShouldCreateFile(true);

  SetShouldCreateDbEntry(true);
  OfflinePageItem page1 = AddPage(kLastNNamespace, temporary_dir());
  SetShouldCreateDbEntry(false);
  OfflinePageItem page2 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(temporary_dir()));

  auto task = std::make_unique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir()));
  EXPECT_FALSE(IsPageRemovedFromBothPlaces(page1));
  EXPECT_TRUE(IsPageRemovedFromBothPlaces(page2));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingArchiveFileCount",
      0);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingDbEntryCount", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
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
  SetShouldCreateDbEntry(true);

  SetShouldCreateFile(true);
  OfflinePageItem page1 = AddPage(kLastNNamespace, temporary_dir());
  SetShouldCreateFile(false);
  OfflinePageItem page2 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir()));

  auto task = std::make_unique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir()));
  EXPECT_FALSE(IsPageRemovedFromBothPlaces(page1));
  EXPECT_TRUE(IsPageRemovedFromBothPlaces(page2));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingDbEntryCount", 0);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingArchiveFileCount", 1,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
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
  SetShouldCreateDbEntry(true);
  SetShouldCreateFile(true);
  OfflinePageItem page1 = AddPage(kLastNNamespace, temporary_dir());
  SetShouldCreateDbEntry(false);
  SetShouldCreateFile(true);
  OfflinePageItem page2 = AddPage(kLastNNamespace, temporary_dir());
  SetShouldCreateDbEntry(true);
  SetShouldCreateFile(false);
  OfflinePageItem page3 = AddPage(kLastNNamespace, temporary_dir());

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(temporary_dir()));

  auto task = std::make_unique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), temporary_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir()));
  EXPECT_FALSE(IsPageRemovedFromBothPlaces(page1));
  EXPECT_TRUE(IsPageRemovedFromBothPlaces(page2));
  EXPECT_TRUE(IsPageRemovedFromBothPlaces(page3));
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingArchiveFileCount", 1,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingDbEntryCount", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
}

}  // namespace offline_pages
