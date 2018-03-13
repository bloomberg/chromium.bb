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
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class TemporaryPagesConsistencyCheckTaskTest : public ModelTaskTestBase {
 public:
  ~TemporaryPagesConsistencyCheckTaskTest() override {}

  void SetUp() override;

  bool IsPageRemovedFromBothPlaces(const OfflinePageItem& page);

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

void TemporaryPagesConsistencyCheckTaskTest::SetUp() {
  ModelTaskTestBase::SetUp();
  generator()->SetNamespace(kLastNNamespace);
  histogram_tester_ = std::make_unique<base::HistogramTester>();
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

  OfflinePageItem page1 = AddPage();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(TemporaryDir()));

  auto task = std::make_unique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), TemporaryDir());
  RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
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

  OfflinePageItem page1 = AddPage();
  OfflinePageItem page2 = AddPageWithoutFile();

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));

  auto task = std::make_unique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), TemporaryDir());
  RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
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
  OfflinePageItem page1 = AddPage();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page3 = AddPageWithoutFile();

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(TemporaryDir()));

  auto task = std::make_unique<TemporaryPagesConsistencyCheckTask>(
      store(), policy_controller(), TemporaryDir());
  RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
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
