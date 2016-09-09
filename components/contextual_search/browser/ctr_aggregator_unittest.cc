// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/browser/ctr_aggregator.h"

#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;

namespace {
const int kTestWeek = 2500;
}

namespace contextual_search {

class CTRAggregatorTest : public testing::Test {
 public:
  CTRAggregatorTest() {}
  ~CTRAggregatorTest() override {}

  class WeeklyActivityStorageStub : public WeeklyActivityStorage {
   public:
    WeeklyActivityStorageStub();

   private:
    int ReadStorage(std::string storage_bucket) override;
    void WriteStorage(std::string storage_key, int value) override;

    typedef std::unordered_map<std::string, int> hashmap;
    hashmap weeks_;
  };

  // Test helpers
  void Fill4Weeks();  // Fill 4 weeks with 2 impressions, 1 click.

  // The class under test.
  std::unique_ptr<CTRAggregator> aggregator_;

 protected:
  // The storage stub.
  std::unique_ptr<WeeklyActivityStorage> storage_;

  void SetUp() override {
    storage_.reset(new WeeklyActivityStorageStub());
    aggregator_.reset(new CTRAggregator(*storage_.get(), kTestWeek));
  }

  void TearDown() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CTRAggregatorTest);
};

CTRAggregatorTest::WeeklyActivityStorageStub::WeeklyActivityStorageStub()
    : WeeklyActivityStorage(4) {}

int CTRAggregatorTest::WeeklyActivityStorageStub::ReadStorage(
    std::string storage_bucket) {
  return weeks_[storage_bucket];
}

void CTRAggregatorTest::WeeklyActivityStorageStub::WriteStorage(
    std::string storage_bucket,
    int value) {
  weeks_[storage_bucket] = value;
}

void CTRAggregatorTest::Fill4Weeks() {
  int weeks_to_record = 4;
  for (int i = 0; i < weeks_to_record; i++) {
    aggregator_->RecordImpression(true);
    aggregator_->RecordImpression(false);
    EXPECT_FALSE(aggregator_->HasPrevious28DayData());
    aggregator_->IncrementWeek(1);
  }
  EXPECT_TRUE(aggregator_->HasPrevious28DayData());
}

// NaN has the property that it is not equal to itself.
#define EXPECT_NAN(x) EXPECT_NE(x, x)

TEST_F(CTRAggregatorTest, SimpleOperationTest) {
  aggregator_->RecordImpression(true);
  aggregator_->RecordImpression(false);
  EXPECT_FALSE(aggregator_->HasPreviousWeekData());
  EXPECT_EQ(0, aggregator_->GetPreviousWeekImpressions());
  EXPECT_NAN(aggregator_->GetPreviousWeekCTR());

  aggregator_->IncrementWeek(1);
  EXPECT_TRUE(aggregator_->HasPreviousWeekData());
  EXPECT_EQ(2, aggregator_->GetPreviousWeekImpressions());
  EXPECT_FLOAT_EQ(0.5f, aggregator_->GetPreviousWeekCTR());
}

TEST_F(CTRAggregatorTest, MultiWeekTest) {
  Fill4Weeks();
  aggregator_->RecordImpression(false);
  aggregator_->IncrementWeek(1);
  EXPECT_TRUE(aggregator_->HasPrevious28DayData());
  EXPECT_FLOAT_EQ(static_cast<float>(3.0 / 7),
                  aggregator_->GetPrevious28DayCTR());
  aggregator_->RecordImpression(false);
  aggregator_->IncrementWeek(1);
  EXPECT_TRUE(aggregator_->HasPrevious28DayData());
  EXPECT_FLOAT_EQ(static_cast<float>(2.0 / 6),
                  aggregator_->GetPrevious28DayCTR());
}

TEST_F(CTRAggregatorTest, SkipOneWeekTest) {
  Fill4Weeks();
  aggregator_->IncrementWeek(1);
  EXPECT_EQ(0, aggregator_->GetPreviousWeekCTR());
  EXPECT_FLOAT_EQ(static_cast<float>(3.0 / 6),
                  aggregator_->GetPrevious28DayCTR());
}

TEST_F(CTRAggregatorTest, SkipThreeWeeksTest) {
  Fill4Weeks();
  aggregator_->IncrementWeek(3);
  EXPECT_EQ(0, aggregator_->GetPreviousWeekCTR());
  EXPECT_FLOAT_EQ(static_cast<float>(1.0 / 2),
                  aggregator_->GetPrevious28DayCTR());
}

TEST_F(CTRAggregatorTest, SkipFourWeeksTest) {
  Fill4Weeks();
  aggregator_->IncrementWeek(4);
  EXPECT_EQ(0, aggregator_->GetPreviousWeekCTR());
  EXPECT_EQ(0, aggregator_->GetPrevious28DayCTR());
}

}  // namespace contextual_search
