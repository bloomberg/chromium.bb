// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/clear_storage_task.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/model/offline_page_test_util.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using ClearStorageResult = ClearStorageTask::ClearStorageResult;
using StorageStats = ArchiveManager::StorageStats;

namespace {

const GURL kTestUrl("http://example.com");
const int64_t kTestFileSize = 1 << 19;              // Make a page 512KB.
const int64_t kFreeSpaceNormal = 1000 * (1 << 20);  // 1000MB free space.

enum TestOptions {
  DEFAULT = 1 << 0,
  DELETE_FAILURE = 1 << 1,
};

struct PageSettings {
  std::string name_space;
  int fresh_page_count;
  int expired_page_count;
};

class TestArchiveManager : public ArchiveManager {
 public:
  explicit TestArchiveManager(OfflinePageMetadataStoreTestUtil* store_test_util)
      : free_space_(kFreeSpaceNormal), store_test_util_(store_test_util) {}

  void GetStorageStats(
      const base::Callback<void(const StorageStats& storage_stats)>& callback)
      const override {
    callback.Run(
        {free_space_, store_test_util_->GetPageCount() * kTestFileSize});
  }

  void SetFreeSpace(int64_t free_space) { free_space_ = free_space; }

 private:
  int64_t free_space_;
  OfflinePageMetadataStoreTestUtil* store_test_util_;
};

}  // namespace

class ClearStorageTaskTest
    : public testing::Test,
      public base::SupportsWeakPtr<ClearStorageTaskTest> {
 public:
  ClearStorageTaskTest();
  ~ClearStorageTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  void Initialize(const std::vector<PageSettings>& settings,
                  base::SimpleTestClock* clock,
                  TestOptions options = TestOptions::DEFAULT);
  void OnClearStorageDone(size_t cleared_page_count, ClearStorageResult result);
  void AddPages(const PageSettings& setting, base::SimpleTestClock* clock_ptr);
  void RunClearStorageTask(const base::Time& start_time);

  void SetFreeSpace(int64_t free_space) {
    archive_manager_->SetFreeSpace(free_space);
  }

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TestTaskRunner* runner() { return &runner_; }
  ClientPolicyController* policy_controller() {
    return policy_controller_.get();
  }
  ArchiveManager* archive_manager() { return archive_manager_.get(); }
  const base::FilePath& temp_dir_path() { return temp_dir_.GetPath(); }
  base::SimpleTestClock* clock() { return clock_; }
  size_t last_cleared_page_count() { return last_cleared_page_count_; }
  int total_cleared_times() { return total_cleared_times_; }
  ClearStorageResult last_clear_storage_result() {
    return last_clear_storage_result_;
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;

  std::unique_ptr<ClientPolicyController> policy_controller_;
  std::unique_ptr<TestArchiveManager> archive_manager_;
  base::ScopedTempDir temp_dir_;
  base::SimpleTestClock* clock_;

  size_t last_cleared_page_count_;
  int total_cleared_times_;
  ClearStorageResult last_clear_storage_result_;
};

ClearStorageTaskTest::ClearStorageTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_),
      last_cleared_page_count_(0),
      total_cleared_times_(0),
      last_clear_storage_result_(ClearStorageResult::SUCCESS) {}

ClearStorageTaskTest::~ClearStorageTaskTest() {}

void ClearStorageTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  // Setting up policies for testing.
  policy_controller_ = base::MakeUnique<ClientPolicyController>();
}

void ClearStorageTaskTest::TearDown() {
  store_test_util_.DeleteStore();
  if (temp_dir_.IsValid()) {
    if (!temp_dir_.Delete())
      DVLOG(1) << "temp_dir_ not created";
  }
  task_runner_->RunUntilIdle();
}

void ClearStorageTaskTest::Initialize(
    const std::vector<PageSettings>& page_settings,
    base::SimpleTestClock* clock,
    TestOptions options) {
  generator()->SetFileSize(kTestFileSize);

  // Adding pages based on |page_settings|.
  for (const auto& setting : page_settings)
    AddPages(setting, clock);
  archive_manager_.reset(new TestArchiveManager(store_test_util()));
}

void ClearStorageTaskTest::OnClearStorageDone(size_t cleared_page_count,
                                              ClearStorageResult result) {
  last_cleared_page_count_ = cleared_page_count;
  last_clear_storage_result_ = result;
  total_cleared_times_++;
}

void ClearStorageTaskTest::AddPages(const PageSettings& setting,
                                    base::SimpleTestClock* clock_ptr) {
  generator()->SetNamespace(setting.name_space);
  generator()->SetArchiveDirectory(temp_dir_path());
  for (int i = 0; i < setting.fresh_page_count; ++i) {
    generator()->SetLastAccessTime(clock_ptr->Now());
    OfflinePageItem page = generator()->CreateItemWithTempFile();
    store_test_util()->InsertItem(page);
  }
  for (int i = 0; i < setting.expired_page_count; ++i) {
    // Make the pages expired.
    generator()->SetLastAccessTime(
        clock_ptr->Now() - policy_controller_->GetPolicy(setting.name_space)
                               .lifetime_policy.expiration_period);
    OfflinePageItem page = generator()->CreateItemWithTempFile();
    store_test_util()->InsertItem(page);
  }
}

void ClearStorageTaskTest::RunClearStorageTask(const base::Time& start_time) {
  auto task = base::MakeUnique<ClearStorageTask>(
      store(), archive_manager(), policy_controller(), start_time,
      base::Bind(&ClearStorageTaskTest::OnClearStorageDone, AsWeakPtr()));

  runner()->RunTask(std::move(task));
}

TEST_F(ClearStorageTaskTest, ClearPagesLessThanLimit) {
  auto clock = base::MakeUnique<base::SimpleTestClock>();
  clock->SetNow(base::Time::Now());
  Initialize({{kBookmarkNamespace, 1, 1}, {kLastNNamespace, 1, 1}},
             clock.get());

  clock->Advance(base::TimeDelta::FromMinutes(5));
  base::Time start_time = clock->Now();

  RunClearStorageTask(start_time);

  // In total there're 2 expired pages so they'll be cleared successfully.
  // There will be 2 pages remaining in the store, and make sure their files
  // weren't cleared.
  EXPECT_EQ(2UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_util::GetFileCountInDirectory(temp_dir_path()));
}

TEST_F(ClearStorageTaskTest, ClearPagesMoreFreshPages) {
  auto clock = base::MakeUnique<base::SimpleTestClock>();
  clock->SetNow(base::Time::Now());
  Initialize({{kBookmarkNamespace, 30, 0}, {kLastNNamespace, 100, 1}},
             clock.get());

  clock->Advance(base::TimeDelta::FromMinutes(5));
  base::Time start_time = clock->Now();

  RunClearStorageTask(start_time);

  // In total there's 1 expired page so it'll be cleared successfully.
  // There will be (30 + 100) pages remaining in the store, and make sure their
  // files weren't cleared.
  EXPECT_EQ(1UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(130LL, store_test_util()->GetPageCount());
  EXPECT_EQ(130UL, test_util::GetFileCountInDirectory(temp_dir_path()));
}

TEST_F(ClearStorageTaskTest, TryClearPersistentPages) {
  auto clock = base::MakeUnique<base::SimpleTestClock>();
  clock->SetNow(base::Time::Now());
  Initialize({{kDownloadNamespace, 20, 0}}, clock.get());

  clock->Advance(base::TimeDelta::FromDays(367));
  base::Time start_time = clock->Now();

  RunClearStorageTask(start_time);

  // There's 20 pages and the clock advances for more than a year.
  // No pages should be deleted since they're all persistent pages.
  EXPECT_EQ(0UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(20LL, store_test_util()->GetPageCount());
  EXPECT_EQ(20UL, test_util::GetFileCountInDirectory(temp_dir_path()));
}

TEST_F(ClearStorageTaskTest, TryClearPersistentPagesWithStoragePressure) {
  auto clock = base::MakeUnique<base::SimpleTestClock>();
  clock->SetNow(base::Time::Now());
  // Sets the free space with 1KB.
  Initialize({{kDownloadNamespace, 20, 0}}, clock.get());
  SetFreeSpace(1024);

  clock->Advance(base::TimeDelta::FromDays(367));
  base::Time start_time = clock->Now();

  RunClearStorageTask(start_time);

  // There're 20 pages and the clock advances for more than a year.
  // No pages should be deleted since they're all persistent pages.
  EXPECT_EQ(0UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(20LL, store_test_util()->GetPageCount());
  EXPECT_EQ(20UL, test_util::GetFileCountInDirectory(temp_dir_path()));
}

TEST_F(ClearStorageTaskTest, ClearMultipleTimes) {
  auto clock = base::MakeUnique<base::SimpleTestClock>();
  clock->SetNow(base::Time::Now());
  // Initializing with 30 unexpired and 0 expired pages in bookmark namespace,
  // 20 unexpired and 1 expired pages in last_n namespace, and 40 persistent
  // pages in download namespace. Free space on the disk is 200MB.
  Initialize({{kBookmarkNamespace, 30, 0},
              {kLastNNamespace, 20, 1},
              {kDownloadNamespace, 40, 0}},
             clock.get());

  clock->Advance(base::TimeDelta::FromMinutes(30));
  base::Time start_time = clock->Now();

  RunClearStorageTask(start_time);

  // There's only 1 expired pages, so it will be cleared. There will be (30 +
  // 20 + 40) pages remaining.
  EXPECT_EQ(1UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(90LL, store_test_util()->GetPageCount());
  EXPECT_EQ(90UL, test_util::GetFileCountInDirectory(temp_dir_path()));

  clock->SetNow(start_time);
  // Advance the clock by the expiration period of last_n namespace, all pages
  // left in that namespace should be expired.
  LifetimePolicy last_n_policy =
      policy_controller()->GetPolicy(kLastNNamespace).lifetime_policy;
  clock->Advance(last_n_policy.expiration_period);
  start_time = clock->Now();

  RunClearStorageTask(start_time);

  // All pages in last_n namespace should be cleared. And only 70 pages
  // remaining after the clearing.
  EXPECT_EQ(20UL, last_cleared_page_count());
  EXPECT_EQ(2, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(70LL, store_test_util()->GetPageCount());
  EXPECT_EQ(70UL, test_util::GetFileCountInDirectory(temp_dir_path()));

  clock->SetNow(start_time);
  // Advance the clock by 1 ms, there's no change in pages so the attempt to
  // clear storage should be unnecessary.
  clock->Advance(base::TimeDelta::FromMilliseconds(1));
  start_time = clock->Now();

  RunClearStorageTask(start_time);

  // The clearing attempt is unnecessary.
  EXPECT_EQ(0UL, last_cleared_page_count());
  EXPECT_EQ(3, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(70LL, store_test_util()->GetPageCount());
  EXPECT_EQ(70UL, test_util::GetFileCountInDirectory(temp_dir_path()));

  clock->SetNow(start_time);
  // Adding more fresh pages in bookmark namespace to make storage usage exceed
  // limit, so even if only 5 minutes passed from last clearing, this will still
  // clear some pages.
  // Free storage space is 200MB and all pages take 270 * 0.5MB = 135MB (while
  // temporary pages takes 230 * 0.5MB = 115MB), which is over (135MB + 200MB)
  // * 0.3 = 100.5MB. In order to bring the storage usage down to (135MB +
  // 200MB) * 0.1 = 33.5MB, (115MB - 33.5MB) needs to be released, which is 163
  // temporary pages to be cleared.
  AddPages({kBookmarkNamespace, 200, 0}, clock.get());
  SetFreeSpace(200 * (1 << 20));
  clock->Advance(base::TimeDelta::FromMinutes(5));
  start_time = clock->Now();

  RunClearStorageTask(start_time);

  // All pages in last_n namespace should be cleared. And only 70 pages
  // remaining after the clearing.
  EXPECT_EQ(163UL, last_cleared_page_count());
  EXPECT_EQ(4, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(107LL, store_test_util()->GetPageCount());
  EXPECT_EQ(107UL, test_util::GetFileCountInDirectory(temp_dir_path()));

  clock->SetNow(start_time);
  // Advance the clock by 300 days, in order to expire all temporary pages. Only
  // 67 temporary pages are left from the last clearing.
  clock->Advance(base::TimeDelta::FromDays(300));
  start_time = clock->Now();

  RunClearStorageTask(start_time);

  // All temporary pages should be cleared by now.
  EXPECT_EQ(67UL, last_cleared_page_count());
  EXPECT_EQ(5, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(40LL, store_test_util()->GetPageCount());
  EXPECT_EQ(40UL, test_util::GetFileCountInDirectory(temp_dir_path()));
}

}  // namespace offline_pages
