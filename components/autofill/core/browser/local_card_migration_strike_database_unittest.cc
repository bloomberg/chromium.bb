// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/local_card_migration_strike_database.h"

#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_local_card_migration_strike_database.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class LocalCardMigrationStrikeDatabaseTest : public ::testing::Test {
 public:
  LocalCardMigrationStrikeDatabaseTest()
      : strike_database_(new StrikeDatabase(InitFilePath())) {}

 protected:
  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestLocalCardMigrationStrikeDatabase strike_database_;

 private:
  static const base::FilePath InitFilePath() {
    base::ScopedTempDir temp_dir_;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    const base::FilePath file_path =
        temp_dir_.GetPath().AppendASCII("StrikeDatabaseTest");
    return file_path;
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(LocalCardMigrationStrikeDatabaseTest, MaxStrikesLimitReachedTest) {
  EXPECT_EQ(false, strike_database_.IsMaxStrikesLimitReached());
  // 3 strikes added.
  strike_database_.AddStrikes(3);
  EXPECT_EQ(false, strike_database_.IsMaxStrikesLimitReached());
  // 4 strike added, total strike count is 7.
  strike_database_.AddStrikes(4);
  EXPECT_EQ(true, strike_database_.IsMaxStrikesLimitReached());
}

TEST_F(LocalCardMigrationStrikeDatabaseTest,
       LocalCardMigrationNthStrikeAddedHistogram) {
  // 2 strikes logged.
  strike_database_.AddStrikes(2);
  strike_database_.RemoveStrikes(2);
  // 1 strike logged.
  strike_database_.AddStrike();
  // 2 strikes logged.
  strike_database_.AddStrike();
  std::vector<base::Bucket> buckets = GetHistogramTester()->GetAllSamples(
      "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration");
  // There should be two buckets, for strike counts of 1 and 2.
  ASSERT_EQ(2U, buckets.size());
  // Bucket for 1 strike should have count of 1.
  EXPECT_EQ(1, buckets[0].count);
  // Bucket for 2 strikes should have count of 2.
  EXPECT_EQ(2, buckets[1].count);
}

TEST_F(LocalCardMigrationStrikeDatabaseTest,
       AddStrikeForZeroAndNonZeroStrikesTest) {
  EXPECT_EQ(0, strike_database_.GetStrikes());
  strike_database_.AddStrike();
  EXPECT_EQ(1, strike_database_.GetStrikes());
  strike_database_.AddStrikes(2);
  EXPECT_EQ(3, strike_database_.GetStrikes());
}

TEST_F(LocalCardMigrationStrikeDatabaseTest,
       ClearStrikesForNonZeroStrikesTest) {
  strike_database_.AddStrikes(3);
  EXPECT_EQ(3, strike_database_.GetStrikes());
  strike_database_.ClearStrikes();
  EXPECT_EQ(0, strike_database_.GetStrikes());
}

TEST_F(LocalCardMigrationStrikeDatabaseTest, ClearStrikesForZeroStrikesTest) {
  strike_database_.ClearStrikes();
  EXPECT_EQ(0, strike_database_.GetStrikes());
}

TEST_F(LocalCardMigrationStrikeDatabaseTest, RemoveExpiredStrikesTest) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(AutofillClock::Now());
  strike_database_.AddStrikes(2);
  EXPECT_EQ(2, strike_database_.GetStrikes());

  // Advance clock to past expiry time.
  test_clock.Advance(base::TimeDelta::FromMicroseconds(
      strike_database_.GetExpiryTimeMicros() + 1));

  // One strike should be removed.
  strike_database_.RemoveExpiredStrikes();
  EXPECT_EQ(1, strike_database_.GetStrikes());
}

}  // namespace autofill
