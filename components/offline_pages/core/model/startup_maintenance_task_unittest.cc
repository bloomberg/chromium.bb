// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/startup_maintenance_task.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const int64_t kTestFileSize = 512 * (1 << 10);  // Default file size is 512KB.

// Used for checking if the page is still present in database and/or filesystem.
enum class PagePresence {
  BOTH_DB_AND_FILESYSTEM,
  DB_ONLY,
  FILESYSTEM_ONLY,
  NONE,
};

}  // namespace

class StartupMaintenanceTaskTest : public ModelTaskTestBase {
 public:
  StartupMaintenanceTaskTest();
  ~StartupMaintenanceTaskTest() override;

  void SetUp() override;
  PagePresence CheckPagePresence(const OfflinePageItem& page);

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

StartupMaintenanceTaskTest::StartupMaintenanceTaskTest() {}

StartupMaintenanceTaskTest::~StartupMaintenanceTaskTest() {}

void StartupMaintenanceTaskTest::SetUp() {
  ModelTaskTestBase::SetUp();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

PagePresence StartupMaintenanceTaskTest::CheckPagePresence(
    const OfflinePageItem& page) {
  if (base::PathExists(page.file_path)) {
    if (store_test_util()->GetPageByOfflineId(page.offline_id))
      return PagePresence::BOTH_DB_AND_FILESYSTEM;
    else
      return PagePresence::FILESYSTEM_ONLY;
  } else {
    if (store_test_util()->GetPageByOfflineId(page.offline_id))
      return PagePresence::DB_ONLY;
    else
      return PagePresence::NONE;
  }
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
TEST_F(StartupMaintenanceTaskTest, MAYBE_TestDeletePageInLegacyArchivesDir) {
  generator()->SetArchiveDirectory(PrivateDir());
  generator()->SetNamespace(kLastNNamespace);
  OfflinePageItem temporary_page = AddPage();
  generator()->SetNamespace(kDownloadNamespace);
  OfflinePageItem persistent_page = AddPage();
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(temporary_page));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(persistent_page));

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(PrivateDir()));

  auto task = std::make_unique<StartupMaintenanceTask>(
      store(), archive_manager(), policy_controller());
  RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_FALSE(
      store_test_util()->GetPageByOfflineId(temporary_page.offline_id));
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeleteFileWithoutDbEntry DISABLED_TestDeleteFileWithoutDbEntry
#else
#define MAYBE_TestDeleteFileWithoutDbEntry TestDeleteFileWithoutDbEntry
#endif
TEST_F(StartupMaintenanceTaskTest, MAYBE_TestDeleteFileWithoutDbEntry) {
  // Only the files without DB entries will be deleted.
  generator()->SetNamespace(kLastNNamespace);
  generator()->SetArchiveDirectory(TemporaryDir());
  OfflinePageItem temporary_page1 = AddPage();
  OfflinePageItem temporary_page2 = AddPageWithoutDBEntry();

  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetArchiveDirectory(PrivateDir());
  OfflinePageItem persistent_page1 = AddPage();
  OfflinePageItem persistent_page2 = AddPageWithoutDBEntry();

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::FILESYSTEM_ONLY, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::FILESYSTEM_ONLY, CheckPagePresence(persistent_page2));

  auto task = std::make_unique<StartupMaintenanceTask>(
      store(), archive_manager(), policy_controller());
  RunTask(std::move(task));

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(temporary_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(persistent_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(persistent_page2));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingArchiveFileCount",
      0);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingDbEntryCount", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ConsistencyCheck.Persistent.PagesMissingArchiveFileCount",
      0);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Persistent.PagesMissingDbEntryCount", 1,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Persistent.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeleteDbEntryWithoutFile DISABLED_TestDeleteDbEntryWithoutFile
#else
#define MAYBE_TestDeleteDbEntryWithoutFile TestDeleteDbEntryWithoutFile
#endif
TEST_F(StartupMaintenanceTaskTest, MAYBE_TestDeleteDbEntryWithoutFile) {
  // Only the DB entries without associating files will be deleted.
  generator()->SetNamespace(kLastNNamespace);
  generator()->SetArchiveDirectory(TemporaryDir());
  OfflinePageItem temporary_page1 = AddPage();
  OfflinePageItem temporary_page2 = AddPageWithoutFile();

  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetArchiveDirectory(PrivateDir());
  OfflinePageItem persistent_page1 = AddPage();
  OfflinePageItem persistent_page2 = AddPageWithoutFile();

  EXPECT_EQ(4LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::DB_ONLY, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::DB_ONLY, CheckPagePresence(persistent_page2));

  auto task = std::make_unique<StartupMaintenanceTask>(
      store(), archive_manager(), policy_controller());
  RunTask(std::move(task));

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(temporary_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(persistent_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(persistent_page2));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingDbEntryCount", 0);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingArchiveFileCount", 1,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ConsistencyCheck.Persistent.PagesMissingDbEntryCount", 0);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Persistent.PagesMissingArchiveFileCount",
      1, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Persistent.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_CombinedTest DISABLED_CombinedTest
#else
#define MAYBE_CombinedTest CombinedTest
#endif
TEST_F(StartupMaintenanceTaskTest, MAYBE_CombinedTest) {
  // Adding a bunch of pages with different setups for temporary pages.
  // After the consistency check, only page1 will exist.
  generator()->SetNamespace(kLastNNamespace);
  generator()->SetArchiveDirectory(TemporaryDir());
  OfflinePageItem page1 = AddPage();
  OfflinePageItem page2 = AddPageWithoutDBEntry();
  OfflinePageItem page3 = AddPageWithoutFile();
  // Adding a bunch of pages with different setups for persistent pages.
  // After the consistency check, only page4 will exist.
  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetArchiveDirectory(PrivateDir());
  OfflinePageItem page4 = AddPage();
  OfflinePageItem page5 = AddPageWithoutDBEntry();
  OfflinePageItem page6 = AddPageWithoutFile();

  EXPECT_EQ(4LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::FILESYSTEM_ONLY, CheckPagePresence(page2));
  EXPECT_EQ(PagePresence::DB_ONLY, CheckPagePresence(page3));
  EXPECT_EQ(PagePresence::FILESYSTEM_ONLY, CheckPagePresence(page5));
  EXPECT_EQ(PagePresence::DB_ONLY, CheckPagePresence(page6));

  auto task = std::make_unique<StartupMaintenanceTask>(
      store(), archive_manager(), policy_controller());
  RunTask(std::move(task));

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM, CheckPagePresence(page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(page2));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(page3));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM, CheckPagePresence(page4));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(page5));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(page6));
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingArchiveFileCount", 1,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingDbEntryCount", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Persistent.PagesMissingArchiveFileCount",
      1, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Persistent.PagesMissingDbEntryCount", 1,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Persistent.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
}

TEST_F(StartupMaintenanceTaskTest, TestKeepingNonMhtmlFile) {
  // Create an offline page with mhtml extension but has no DB entry.
  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetArchiveDirectory(PrivateDir());
  OfflinePageItem page1 = AddPageWithoutDBEntry();
  // Create a file with non-mhtml extension.
  base::FilePath path;
  base::CreateTemporaryFileInDir(PrivateDir(), &path);
  base::FilePath mp3_path = path.AddExtension(FILE_PATH_LITERAL("mp3"));
  EXPECT_TRUE(base::Move(path, mp3_path));

  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(PrivateDir()));

  auto task = std::make_unique<StartupMaintenanceTask>(
      store(), archive_manager(), policy_controller());
  RunTask(std::move(task));

  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(page1));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ConsistencyCheck.Persistent.PagesMissingArchiveFileCount",
      0);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Persistent.PagesMissingDbEntryCount", 1,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Persistent.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
}

TEST_F(StartupMaintenanceTaskTest, TestReportStorageUsage) {
  generator()->SetFileSize(kTestFileSize);
  std::vector<std::string> namespaces = policy_controller()->GetAllNamespaces();

  // Adding pages into each namespace.
  for (const auto& name_space : namespaces) {
    // Set the correct namespace for generated pages, also put them into the
    // correct directories, otherwise they might be cleaned based on consistency
    // check.
    generator()->SetNamespace(name_space);
    if (policy_controller()->IsRemovedOnCacheReset(name_space))
      generator()->SetArchiveDirectory(TemporaryDir());
    else
      generator()->SetArchiveDirectory(PrivateDir());

    // For each namespace, insert pages based on the length of the namespace, so
    // that we don't need to have const values here.
    for (size_t count = 0; count < name_space.length(); ++count)
      AddPage();
  }

  auto task = std::make_unique<StartupMaintenanceTask>(
      store(), archive_manager(), policy_controller());
  RunTask(std::move(task));

  // For each namespace, check if the storage usage was correctly reported,
  // since the value is reported with KiB as unit, divide it by 1024 here.
  for (const auto& name_space : namespaces) {
    histogram_tester()->ExpectUniqueSample(
        "OfflinePages.ClearStoragePreRunUsage2." + name_space,
        name_space.length() * kTestFileSize / 1024, 1);
  }
}

}  // namespace offline_pages
