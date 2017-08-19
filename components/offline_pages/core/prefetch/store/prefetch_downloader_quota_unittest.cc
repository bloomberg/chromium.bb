// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_downloader_quota.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class PrefetchDownloaderQuotaTest : public testing::Test {
 public:
  PrefetchDownloaderQuotaTest();
  ~PrefetchDownloaderQuotaTest() override = default;

  void SetUp() override { store_test_util_.BuildStoreInMemory(); }

  void TearDown() override { store_test_util_.DeleteStore(); }

  PrefetchStore* store() { return store_test_util_.store(); }

  PrefetchStoreTestUtil* store_util() { return &store_test_util_; }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  PrefetchStoreTestUtil store_test_util_;
};

PrefetchDownloaderQuotaTest::PrefetchDownloaderQuotaTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_) {}

TEST_F(PrefetchDownloaderQuotaTest, GetQuotaWithNothingStored) {
  EXPECT_EQ(PrefetchDownloaderQuota::kMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());
}

TEST_F(PrefetchDownloaderQuotaTest, SetQuotaToZeroAndRead) {
  EXPECT_TRUE(store_util()->SetPrefetchQuota(0));
  EXPECT_EQ(0, store_util()->GetPrefetchQuota());
}

TEST_F(PrefetchDownloaderQuotaTest, SetQuotaToLessThanZero) {
  EXPECT_TRUE(store_util()->SetPrefetchQuota(-10000LL));
  EXPECT_EQ(0, store_util()->GetPrefetchQuota());
}

TEST_F(PrefetchDownloaderQuotaTest, SetQuotaToMoreThanMaximum) {
  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      2 * PrefetchDownloaderQuota::kMaxDailyQuotaBytes));
  EXPECT_EQ(PrefetchDownloaderQuota::kMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());
}

// This test verifies that advancing time by 12 hours (half of quota period),
// recovers half of the quota, withing limits.
TEST_F(PrefetchDownloaderQuotaTest, CheckQuotaForHalfDay) {
  // Start with no quota, and wait for half a day, we should have half of quota.
  EXPECT_TRUE(store_util()->SetPrefetchQuota(0));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(12));
  EXPECT_EQ(PrefetchDownloaderQuota::kMaxDailyQuotaBytes / 2,
            store_util()->GetPrefetchQuota());

  // Now start with quota set to 1/4. Expecting 3/4 of quota.
  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      PrefetchDownloaderQuota::kMaxDailyQuotaBytes / 4));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(12));
  EXPECT_EQ(3 * PrefetchDownloaderQuota::kMaxDailyQuotaBytes / 4,
            store_util()->GetPrefetchQuota());

  // Now start with quota set to 1/2. Expects full quota.
  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      PrefetchDownloaderQuota::kMaxDailyQuotaBytes / 2));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(12));
  EXPECT_EQ(PrefetchDownloaderQuota::kMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());

  // Now start with quota set to 3/4. Expects a limit on quota of daily quota
  // maximum.
  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      3 * PrefetchDownloaderQuota::kMaxDailyQuotaBytes / 4));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(12));
  EXPECT_EQ(PrefetchDownloaderQuota::kMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());
}

// This test deals with small and extreme time change scenarios.
TEST_F(PrefetchDownloaderQuotaTest, CheckQuotaAfterTimeChange) {
  EXPECT_TRUE(store_util()->SetPrefetchQuota(0));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(-1));
  EXPECT_EQ(0, store_util()->GetPrefetchQuota());

  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      PrefetchDownloaderQuota::kMaxDailyQuotaBytes));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(1));
  EXPECT_EQ(PrefetchDownloaderQuota::kMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());

  // When going back in time, we get a situation where quota would be negative,
  // we detect it and write back a value of 0 with that date.
  store_util()->clock()->Advance(base::TimeDelta::FromDays(-365));
  EXPECT_EQ(0, store_util()->GetPrefetchQuota());
  // Above implies that after another day our quota is back to full daily max.
  store_util()->clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(PrefetchDownloaderQuota::kMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());
}

}  // namespace offline_pages
