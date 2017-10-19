// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_storage_manager.h"

#include <stdint.h>
#include <map>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model_impl.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using LifetimePolicy = offline_pages::LifetimePolicy;
using ClearStorageResult =
    offline_pages::OfflinePageStorageManager::ClearStorageResult;
using StorageStats = offline_pages::ArchiveManager::StorageStats;

namespace offline_pages {

namespace {
const char kOneDayNamespace[] = "temporary_namespace_1day";
const char kOneWeekNamespace[] = "temporary_namespace_7day";
const char kPersistentNamespace[] = "persistent_namespace";
const GURL kTestUrl("http://example.com");
const base::FilePath::CharType kFilePath[] = FILE_PATH_LITERAL("/data");
const int64_t kTestFileSize = 1 << 19;  // Make a page 512KB.
const int64_t kFreeSpaceNormal = 100 * (1 << 20);

enum TestOptions {
  DEFAULT = 1 << 0,
  DELETE_FAILURE = 1 << 1,
};

struct PageSettings {
  std::string name_space;
  int fresh_pages_count;
  int expired_pages_count;
};

class TestArchiveManager : public ArchiveManager {
 public:
  explicit TestArchiveManager(StorageStats stats) : stats_(stats) {}

  void GetStorageStats(const base::Callback<
                       void(const ArchiveManager::StorageStats& storage_stats)>&
                           callback) const override {
    callback.Run(stats_);
  }

  void SetValues(ArchiveManager::StorageStats stats) { stats_ = stats; }

 private:
  StorageStats stats_;
};

}  // namespace

class OfflinePageTestModel : public OfflinePageModelImpl {
 public:
  OfflinePageTestModel(std::vector<PageSettings> page_settings,
                       base::SimpleTestClock* clock,
                       ClientPolicyController* policy_controller,
                       TestOptions options = TestOptions::DEFAULT)
      : policy_controller_(policy_controller),
        clock_(clock),
        options_(options),
        next_offline_id_(0) {
    policy_controller_->AddPolicyForTest(
        kOneDayNamespace,
        OfflinePageClientPolicyBuilder(kOneDayNamespace,
                                       LifetimePolicy::LifetimeType::TEMPORARY,
                                       kUnlimitedPages, kUnlimitedPages)
            .SetExpirePeriod(base::TimeDelta::FromDays(1)));
    policy_controller_->AddPolicyForTest(
        kOneWeekNamespace,
        OfflinePageClientPolicyBuilder(kOneWeekNamespace,
                                       LifetimePolicy::LifetimeType::TEMPORARY,
                                       kUnlimitedPages, 1)
            .SetExpirePeriod(base::TimeDelta::FromDays(7)));
    policy_controller_->AddPolicyForTest(
        kPersistentNamespace,
        OfflinePageClientPolicyBuilder(kPersistentNamespace,
                                       LifetimePolicy::LifetimeType::PERSISTENT,
                                       kUnlimitedPages, kUnlimitedPages)
            .SetIsRemovedOnCacheReset(false));
    for (const auto& setting : page_settings)
      AddPages(setting);
  }

  ~OfflinePageTestModel() override;

  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override {
    MultipleOfflinePageItemResult pages;
    for (const auto& id_page_pair : pages_)
      pages.push_back(id_page_pair.second);
    callback.Run(pages);
  }

  void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                              const DeletePageCallback& callback) override;

  void AddPages(const PageSettings& setting);

  // The |removed_pages_| would not be cleared in during a test, so the number
  // of removed pages will be accumulative in a single test case.
  const std::vector<OfflinePageItem>& GetRemovedPages() {
    return removed_pages_;
  }

  int64_t GetTemporaryPagesSize() const;

  base::SimpleTestClock* clock() { return clock_; }

  ClientPolicyController* GetPolicyController() override {
    return policy_controller_;
  }

 private:
  std::map<int64_t, OfflinePageItem> pages_;

  std::vector<OfflinePageItem> removed_pages_;

  // Owned by the test.
  ClientPolicyController* policy_controller_;

  base::SimpleTestClock* clock_;

  TestOptions options_;

  int64_t next_offline_id_;
};

void OfflinePageTestModel::DeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {
  if (options_ & TestOptions::DELETE_FAILURE) {
    callback.Run(DeletePageResult::STORE_FAILURE);
    return;
  }
  for (const auto id : offline_ids) {
    removed_pages_.push_back(pages_.at(id));
    pages_.erase(id);
  }
  callback.Run(DeletePageResult::SUCCESS);
}

int64_t OfflinePageTestModel::GetTemporaryPagesSize() const {
  int64_t res = 0;
  for (const auto& id_page_pair : pages_)
    if (policy_controller_->GetPolicy(id_page_pair.second.client_id.name_space)
            .lifetime_policy.lifetime_type ==
        LifetimePolicy::LifetimeType::TEMPORARY)
      res += id_page_pair.second.file_size;
  return res;
}

OfflinePageTestModel::~OfflinePageTestModel() {}

void OfflinePageTestModel::AddPages(const PageSettings& setting) {
  std::string name_space = setting.name_space;
  int fresh_pages_count = setting.fresh_pages_count;
  int expired_pages_count = setting.expired_pages_count;
  base::Time now = clock()->Now();
  // Fresh pages.
  for (int i = 0; i < fresh_pages_count; i++) {
    OfflinePageItem page =
        OfflinePageItem(kTestUrl, next_offline_id_,
                        ClientId(name_space, std::to_string(next_offline_id_)),
                        base::FilePath(kFilePath), kTestFileSize);
    page.last_access_time = now;
    pages_[next_offline_id_] = page;
    next_offline_id_++;
  }
  // Expired pages.
  for (int i = 0; i < expired_pages_count; i++) {
    OfflinePageItem page =
        OfflinePageItem(kTestUrl, next_offline_id_,
                        ClientId(name_space, std::to_string(next_offline_id_)),
                        base::FilePath(kFilePath), kTestFileSize);
    page.last_access_time = now -
                            policy_controller_->GetPolicy(name_space)
                                .lifetime_policy.expiration_period;
    pages_[next_offline_id_] = page;
    next_offline_id_++;
  }
}

class OfflinePageStorageManagerTest : public testing::Test {
 public:
  OfflinePageStorageManagerTest();
  OfflinePageStorageManager* manager() { return manager_.get(); }
  OfflinePageTestModel* model() { return model_.get(); }
  ClientPolicyController* policy_controller() {
    return policy_controller_.get();
  }
  TestArchiveManager* test_archive_manager() { return archive_manager_.get(); }
  void OnPagesCleared(size_t pages_cleared_count, ClearStorageResult result);
  void Initialize(const std::vector<PageSettings>& settings,
                  StorageStats stats = {kFreeSpaceNormal, 0, 0},
                  TestOptions options = TestOptions::DEFAULT);
  void TryClearPages();
  std::string GetStorageUsageHistogramName(const std::string& name_space);

  // Checks that the total sample count for the storage usage histograms are all
  // equal to |count|.
  void CheckTotalCountForAllStorageUsageHistograms(
      base::HistogramBase::Count count);

  // testing::Test
  void TearDown() override;

  base::SimpleTestClock* clock() { return clock_; }
  int last_cleared_page_count() const {
    return static_cast<int>(last_cleared_page_count_);
  }
  int total_cleared_times() const { return total_cleared_times_; }
  ClearStorageResult last_clear_storage_result() const {
    return last_clear_storage_result_;
  }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  std::unique_ptr<OfflinePageStorageManager> manager_;
  std::unique_ptr<OfflinePageTestModel> model_;
  std::unique_ptr<ClientPolicyController> policy_controller_;
  std::unique_ptr<TestArchiveManager> archive_manager_;

  base::SimpleTestClock* clock_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  size_t last_cleared_page_count_;
  int total_cleared_times_;
  ClearStorageResult last_clear_storage_result_;
};

OfflinePageStorageManagerTest::OfflinePageStorageManagerTest()
    : policy_controller_(new ClientPolicyController()),
      last_cleared_page_count_(0),
      total_cleared_times_(0),
      last_clear_storage_result_(ClearStorageResult::SUCCESS) {}

void OfflinePageStorageManagerTest::Initialize(
    const std::vector<PageSettings>& page_settings,
    StorageStats stats,
    TestOptions options) {
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock());
  clock_ = clock.get();
  clock_->SetNow(base::Time::Now());
  model_.reset(new OfflinePageTestModel(page_settings, clock_,
                                        policy_controller_.get(), options));

  if (stats.free_disk_space == 0)
    stats.free_disk_space = kFreeSpaceNormal;
  if (stats.total_archives_size() == 0) {
    stats.temporary_archives_size = model_->GetTemporaryPagesSize();
  }
  archive_manager_.reset(new TestArchiveManager(stats));
  manager_.reset(new OfflinePageStorageManager(
      model_.get(), policy_controller(), archive_manager_.get()));
  manager_->SetClockForTesting(std::move(clock));

  histogram_tester_ = base::MakeUnique<base::HistogramTester>();
}

void OfflinePageStorageManagerTest::TryClearPages() {
  manager()->ClearPagesIfNeeded(base::Bind(
      &OfflinePageStorageManagerTest::OnPagesCleared, base::Unretained(this)));
}

std::string OfflinePageStorageManagerTest::GetStorageUsageHistogramName(
    const std::string& name_space) {
  return "OfflinePages.ClearStoragePreRunUsage." + name_space;
}

void OfflinePageStorageManagerTest::CheckTotalCountForAllStorageUsageHistograms(
    base::HistogramBase::Count count) {
  for (const std::string name_space : policy_controller_->GetAllNamespaces()) {
    std::string histogram_name = GetStorageUsageHistogramName(name_space);
    std::unique_ptr<base::HistogramSamples> samples =
        histogram_tester_->GetHistogramSamplesSinceCreation(histogram_name);
    EXPECT_EQ(count, samples->TotalCount())
        << "Unexpected " << histogram_name << " total sample count";
  }
}

void OfflinePageStorageManagerTest::TearDown() {
  manager_.reset();
  model_.reset();
}

void OfflinePageStorageManagerTest::OnPagesCleared(size_t pages_cleared_count,
                                                   ClearStorageResult result) {
  last_cleared_page_count_ = pages_cleared_count;
  total_cleared_times_++;
  last_clear_storage_result_ = result;
}

TEST_F(OfflinePageStorageManagerTest, TestClearPagesLessThanLimit) {
  Initialize(std::vector<PageSettings>(
      {{kOneWeekNamespace, 1, 1}, {kOneDayNamespace, 1, 1}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(2, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(2, static_cast<int>(model()->GetRemovedPages().size()));
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneWeekNamespace), 2 * 512, 1);
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneDayNamespace), 2 * 512, 1);
  CheckTotalCountForAllStorageUsageHistograms(1);
}

TEST_F(OfflinePageStorageManagerTest, TestClearPagesMoreThanLimit) {
  Initialize(std::vector<PageSettings>(
      {{kOneWeekNamespace, 10, 15}, {kOneDayNamespace, 5, 30}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(45, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(45, static_cast<int>(model()->GetRemovedPages().size()));
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneWeekNamespace), 25 * 512, 1);
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneDayNamespace), 35 * 512, 1);
  CheckTotalCountForAllStorageUsageHistograms(1);
}

TEST_F(OfflinePageStorageManagerTest, TestClearPagesMoreFreshPages) {
  Initialize(std::vector<PageSettings>(
                 {{kOneWeekNamespace, 30, 0}, {kOneDayNamespace, 100, 1}}),
             {1000 * (1 << 20), 0});
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(1, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(1, static_cast<int>(model()->GetRemovedPages().size()));
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneWeekNamespace), 30 * 512, 1);
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneDayNamespace), 100 * 512, 1);
  CheckTotalCountForAllStorageUsageHistograms(1);
}

TEST_F(OfflinePageStorageManagerTest, TestDeletePersistentPages) {
  Initialize(std::vector<PageSettings>({{kPersistentNamespace, 20, 0}}));
  clock()->Advance(base::TimeDelta::FromDays(367));
  TryClearPages();
  EXPECT_EQ(0, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));
  // As there's no temporary pages, no reporting happens.
  CheckTotalCountForAllStorageUsageHistograms(0);
}

TEST_F(OfflinePageStorageManagerTest, TestDeletionFailed) {
  Initialize(std::vector<PageSettings>(
                 {{kOneWeekNamespace, 10, 10}, {kOneDayNamespace, 10, 10}}),
             {kFreeSpaceNormal, 0}, TestOptions::DELETE_FAILURE);
  TryClearPages();
  EXPECT_EQ(20, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::DELETE_FAILURE, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneWeekNamespace), 20 * 512, 1);
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneDayNamespace), 20 * 512, 1);
  CheckTotalCountForAllStorageUsageHistograms(1);
}

TEST_F(OfflinePageStorageManagerTest, TestStorageTimeInterval) {
  Initialize(std::vector<PageSettings>(
      {{kOneWeekNamespace, 10, 10}, {kOneDayNamespace, 10, 10}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  SCOPED_TRACE("Invocation #1 of TryClearPages()");
  TryClearPages();
  EXPECT_EQ(20, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(20, static_cast<int>(model()->GetRemovedPages().size()));
  // This histogram should only report once per launch. Checking here for the
  // first report.
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneWeekNamespace), 20 * 512, 1);
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneDayNamespace), 20 * 512, 1);
  CheckTotalCountForAllStorageUsageHistograms(1);

  // Advance clock so we go over the gap, but no expired pages.
  clock()->Advance(constants::kClearStorageInterval +
                   base::TimeDelta::FromMinutes(1));
  SCOPED_TRACE("Invocation #2 of TryClearPages()");
  TryClearPages();
  EXPECT_EQ(0, last_cleared_page_count());
  EXPECT_EQ(2, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(20, static_cast<int>(model()->GetRemovedPages().size()));

  // Advance clock so we are still in the gap, should be unnecessary.
  clock()->Advance(constants::kClearStorageInterval -
                   base::TimeDelta::FromMinutes(1));
  SCOPED_TRACE("Invocation #3 of TryClearPages()");
  TryClearPages();
  EXPECT_EQ(0, last_cleared_page_count());
  EXPECT_EQ(3, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(20, static_cast<int>(model()->GetRemovedPages().size()));
  // Testing the histogram after the last run to confirm nothing changed.
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneWeekNamespace), 20 * 512, 1);
  histogram_tester()->ExpectUniqueSample(
      GetStorageUsageHistogramName(kOneDayNamespace), 20 * 512, 1);
  CheckTotalCountForAllStorageUsageHistograms(1);
}

TEST_F(OfflinePageStorageManagerTest, TestClearMultipleTimes) {
  Initialize(std::vector<PageSettings>({{kOneWeekNamespace, 30, 0},
                                        {kOneDayNamespace, 100, 1},
                                        {kPersistentNamespace, 40, 0}}),
             {1000 * (1 << 20), 0});
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  LifetimePolicy policy =
      policy_controller()->GetPolicy(kOneDayNamespace).lifetime_policy;

  // Scopes are being used to limit the reach of each SCOPED_TRACE message.
  {
    SCOPED_TRACE("Invocation #1 of TryClearPages()");
    TryClearPages();
    EXPECT_EQ(1, last_cleared_page_count());
    EXPECT_EQ(1, total_cleared_times());
    EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
    EXPECT_EQ(1, static_cast<int>(model()->GetRemovedPages().size()));
    // For this test the once-per-launch flag for the reporting of this
    // histogram will be reset before each clearing run. Note: this histogram
    // always reports the start state of cleaning procedure. It will look as it
    // is always one step behind.
    histogram_tester()->ExpectUniqueSample(
        GetStorageUsageHistogramName(kOneWeekNamespace), 30 * 512, 1);
    histogram_tester()->ExpectUniqueSample(
        GetStorageUsageHistogramName(kOneDayNamespace), 101 * 512, 1);
    histogram_tester()->ExpectUniqueSample(
        GetStorageUsageHistogramName(kPersistentNamespace), 40 * 512, 1);
    CheckTotalCountForAllStorageUsageHistograms(1);
  }

  {
    // Advance the clock by expiration period of last_n namespace, should be
    // expiring all pages left in the namespace.
    clock()->Advance(policy.expiration_period);
    manager()->ResetUsageReportingFlagForTesting();
    SCOPED_TRACE("Invocation #2 of TryClearPages()");
    TryClearPages();
    EXPECT_EQ(100, last_cleared_page_count());
    EXPECT_EQ(2, total_cleared_times());
    EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
    EXPECT_EQ(101, static_cast<int>(model()->GetRemovedPages().size()));
    histogram_tester()->ExpectUniqueSample(
        GetStorageUsageHistogramName(kOneWeekNamespace), 30 * 512, 2);
    histogram_tester()->ExpectUniqueSample(
        GetStorageUsageHistogramName(kOneDayNamespace), 101 * 512, 2);
    histogram_tester()->ExpectUniqueSample(
        GetStorageUsageHistogramName(kPersistentNamespace), 40 * 512, 2);
    CheckTotalCountForAllStorageUsageHistograms(2);
  }

  {
    // Only 1 ms passes and no changes in pages, so no need to clear page.
    clock()->Advance(base::TimeDelta::FromMilliseconds(1));
    manager()->ResetUsageReportingFlagForTesting();
    SCOPED_TRACE("Invocation #3 of TryClearPages()");
    TryClearPages();
    EXPECT_EQ(0, last_cleared_page_count());
    EXPECT_EQ(3, total_cleared_times());
    EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
    EXPECT_EQ(101, static_cast<int>(model()->GetRemovedPages().size()));
    CheckTotalCountForAllStorageUsageHistograms(2);
  }

  {
    // Adding more fresh pages to make it go over limit.
    clock()->Advance(base::TimeDelta::FromMinutes(5));
    model()->AddPages({kOneWeekNamespace, 400, 0});
    int64_t total_size_before = model()->GetTemporaryPagesSize();
    int64_t available_space = 300 * (1 << 20);  // 300 MB
    test_archive_manager()->SetValues(
        {available_space, total_size_before, 40 * kTestFileSize});
    EXPECT_GE(total_size_before, constants::kOfflinePageStorageLimit *
                                     (available_space + total_size_before));
    // Note: No need to reset usage here as for the last run clearing pages was
    // unnecessary.
    SCOPED_TRACE("Invocation #4 of TryClearPages()");
    TryClearPages();
    EXPECT_LE(total_size_before * constants::kOfflinePageStorageClearThreshold,
              model()->GetTemporaryPagesSize());
    EXPECT_EQ(4, total_cleared_times());
    EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
    // Number of removed pages should be what was already deleted until the last
    // step plus the what's needed to get to the 10% of total free space.
    EXPECT_EQ(101 + 327, static_cast<int>(model()->GetRemovedPages().size()));
    histogram_tester()->ExpectBucketCount(
        GetStorageUsageHistogramName(kOneWeekNamespace), 30 * 512, 2);
    histogram_tester()->ExpectBucketCount(
        GetStorageUsageHistogramName(kOneWeekNamespace), 430 * 512, 1);
    histogram_tester()->ExpectBucketCount(
        GetStorageUsageHistogramName(kOneDayNamespace), 101 * 512, 2);
    histogram_tester()->ExpectBucketCount(
        GetStorageUsageHistogramName(kOneDayNamespace), 0, 1);
    histogram_tester()->ExpectUniqueSample(
        GetStorageUsageHistogramName(kPersistentNamespace), 40 * 512, 3);
    CheckTotalCountForAllStorageUsageHistograms(3);
  }

  {
    // After more days, all pages should be deleted.
    clock()->Advance(base::TimeDelta::FromDays(30));
    manager()->ResetUsageReportingFlagForTesting();
    SCOPED_TRACE("Invocation #5 of TryClearPages()");
    TryClearPages();
    EXPECT_EQ(0, model()->GetTemporaryPagesSize());
    EXPECT_EQ(5, total_cleared_times());
    EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
    // Number of removed pages should be all the temporary pages initially
    // created plus 400 more pages added above for bookmark namespace.
    EXPECT_EQ(131 + 400, static_cast<int>(model()->GetRemovedPages().size()));
    histogram_tester()->ExpectBucketCount(
        GetStorageUsageHistogramName(kOneWeekNamespace), 30 * 512, 2);
    histogram_tester()->ExpectBucketCount(
        GetStorageUsageHistogramName(kOneWeekNamespace), 430 * 512, 1);
    histogram_tester()->ExpectBucketCount(
        GetStorageUsageHistogramName(kOneWeekNamespace), 102 * 512, 1);
    histogram_tester()->ExpectBucketCount(
        GetStorageUsageHistogramName(kOneDayNamespace), 101 * 512, 2);
    histogram_tester()->ExpectBucketCount(
        GetStorageUsageHistogramName(kOneDayNamespace), 0, 2);
    histogram_tester()->ExpectUniqueSample(
        GetStorageUsageHistogramName(kPersistentNamespace), 40 * 512, 4);
    CheckTotalCountForAllStorageUsageHistograms(4);
  }
}

}  // namespace offline_pages
