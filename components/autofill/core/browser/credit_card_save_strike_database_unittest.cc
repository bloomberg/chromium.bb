// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/credit_card_save_strike_database.h"

#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_credit_card_save_strike_database.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class CreditCardSaveStrikeDatabaseTest : public ::testing::Test {
 public:
  CreditCardSaveStrikeDatabaseTest() : strike_database_(InitFilePath()) {}

 protected:
  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestCreditCardSaveStrikeDatabase strike_database_;

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

TEST_F(CreditCardSaveStrikeDatabaseTest, GetKeyForCreditCardSaveTest) {
  const std::string last_four = "1234";
  EXPECT_EQ("CreditCardSave__1234", strike_database_.GetKey(last_four));
}

TEST_F(CreditCardSaveStrikeDatabaseTest, GetIdForCreditCardSaveTest) {
  const std::string key = "CreditCardSave__1234";
  EXPECT_EQ("1234", strike_database_.GetIdPartFromKey(key));
}

TEST_F(CreditCardSaveStrikeDatabaseTest, MaxStrikesLimitReachedTest) {
  const std::string last_four = "1234";
  EXPECT_EQ(false, strike_database_.IsMaxStrikesLimitReached(last_four));
  // 1st strike added for |last_four|.
  strike_database_.AddStrike(last_four);
  EXPECT_EQ(false, strike_database_.IsMaxStrikesLimitReached(last_four));
  // 2nd strike added for |last_four|.
  strike_database_.AddStrike(last_four);
  EXPECT_EQ(false, strike_database_.IsMaxStrikesLimitReached(last_four));
  // 3rd strike added for |last_four|.
  strike_database_.AddStrike(last_four);
  EXPECT_EQ(true, strike_database_.IsMaxStrikesLimitReached(last_four));
}

TEST_F(CreditCardSaveStrikeDatabaseTest,
       CreditCardSaveNthStrikeAddedHistogram) {
  const std::string last_four1 = "1234";
  const std::string last_four2 = "9876";
  // 1st strike added for |last_four1|.
  strike_database_.AddStrike(last_four1);
  // 2nd strike added for |last_four1|.
  strike_database_.AddStrike(last_four1);
  // 1st strike added for |last_four2|.
  strike_database_.AddStrike(last_four2);
  std::vector<base::Bucket> buckets = GetHistogramTester()->GetAllSamples(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave");
  // There should be two buckets, one for 1st strike, one for 2nd strike count.
  ASSERT_EQ(2U, buckets.size());
  // Both |last_four1| and |last_four2| have 1st strikes recorded.
  EXPECT_EQ(2, buckets[0].count);
  // Only |last_four1| has 2nd strike recorded.
  EXPECT_EQ(1, buckets[1].count);
}

TEST_F(CreditCardSaveStrikeDatabaseTest,
       AddStrikeForZeroAndNonZeroStrikesTest) {
  const std::string last_four = "1234";
  EXPECT_EQ(0, strike_database_.GetStrikes(last_four));
  strike_database_.AddStrike(last_four);
  EXPECT_EQ(1, strike_database_.GetStrikes(last_four));
  strike_database_.AddStrike(last_four);
  EXPECT_EQ(2, strike_database_.GetStrikes(last_four));
}

TEST_F(CreditCardSaveStrikeDatabaseTest, ClearStrikesForNonZeroStrikesTest) {
  const std::string last_four = "1234";
  strike_database_.AddStrike(last_four);
  EXPECT_EQ(1, strike_database_.GetStrikes(last_four));
  strike_database_.ClearStrikes(last_four);
  EXPECT_EQ(0, strike_database_.GetStrikes(last_four));
}

TEST_F(CreditCardSaveStrikeDatabaseTest, ClearStrikesForZeroStrikesTest) {
  const std::string last_four = "1234";
  strike_database_.ClearStrikes(last_four);
  EXPECT_EQ(0, strike_database_.GetStrikes(last_four));
}

TEST_F(CreditCardSaveStrikeDatabaseTest, RemoveExpiredStrikesOnLoadTest) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(AutofillClock::Now());
  const std::string last_four1 = "1234";
  const std::string last_four2 = "9876";

  StrikeData data1;
  data1.set_num_strikes(2);
  data1.set_last_update_timestamp(
      AutofillClock::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Advance clock to past |data1|'s expiry time.
  test_clock.Advance(base::TimeDelta::FromMicroseconds(
      strike_database_.GetExpiryTimeMicros() + 1));

  StrikeData data2;
  data2.set_num_strikes(2);
  data2.set_last_update_timestamp(
      AutofillClock::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  std::map<std::string, StrikeData> entries;
  entries[strike_database_.GetKey(last_four1)] = data1;
  entries[strike_database_.GetKey(last_four2)] = data2;

  // |data1| should have its most recent strike expire, but |data2| should not.
  strike_database_.OnDatabaseLoadKeysAndEntries(
      true, std::make_unique<std::map<std::string, StrikeData>>(entries));

  EXPECT_EQ(1, strike_database_.GetStrikes(last_four1));
  EXPECT_EQ(2, strike_database_.GetStrikes(last_four2));

  // Advance clock to past both |data1| andd |data2|'s expiry time.
  test_clock.Advance(base::TimeDelta::FromMicroseconds(
      strike_database_.GetExpiryTimeMicros() + 1));

  // |data1| and |data2| should both have its most recent strike expire.
  strike_database_.OnDatabaseLoadKeysAndEntries(
      true, std::make_unique<std::map<std::string, StrikeData>>(entries));

  EXPECT_EQ(0, strike_database_.GetStrikes(last_four1));
  EXPECT_EQ(1, strike_database_.GetStrikes(last_four2));
}

}  // namespace autofill
