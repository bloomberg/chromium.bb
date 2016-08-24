// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_database.h"

#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/budget_service/budget.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const double kDefaultBudget1 = 1.234;
const double kDefaultBudget2 = 2.345;
const double kDefaultExpirationInHours = 72;
const double kDefaultEngagement = 30.0;

const char kTestOrigin[] = "https://example.com";

}  // namespace

class BudgetDatabaseTest : public ::testing::Test {
 public:
  BudgetDatabaseTest()
      : success_(false),
        db_(profile_.GetPath().Append(FILE_PATH_LITERAL("BudgetDabase")),
            base::ThreadTaskRunnerHandle::Get()) {}

  // The BudgetDatabase assumes that a budget will always be queried before it
  // is written to. Use GetBudgetDetails() to pre-populate the cache.
  void SetUp() override { GetBudgetDetails(); }

  void WriteBudgetComplete(base::Closure run_loop_closure, bool success) {
    success_ = success;
    run_loop_closure.Run();
  }

  // Add budget to the origin.
  bool AddBudget(const GURL& origin, double amount) {
    base::RunLoop run_loop;
    db_.AddBudget(origin, amount,
                  base::Bind(&BudgetDatabaseTest::WriteBudgetComplete,
                             base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  // Add engagement based budget to the origin.
  bool AddEngagementBudget(const GURL& origin, double sesScore) {
    base::RunLoop run_loop;
    db_.AddEngagementBudget(
        origin, sesScore,
        base::Bind(&BudgetDatabaseTest::WriteBudgetComplete,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  // Spend budget for the origin.
  bool SpendBudget(const GURL& origin, double amount) {
    base::RunLoop run_loop;
    db_.SpendBudget(origin, amount,
                    base::Bind(&BudgetDatabaseTest::WriteBudgetComplete,
                               base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  void GetBudgetDetailsComplete(
      base::Closure run_loop_closure,
      bool success,
      const BudgetDatabase::BudgetPrediction& prediction) {
    success_ = success;
    // Convert BudgetPrediction to a vector for random access to check values.
    /*
    prediction_.clear();
    for (auto& chunk : prediction)
      prediction_.push_back(chunk);
      */
    prediction_.assign(prediction.begin(), prediction.end());
    run_loop_closure.Run();
  }

  // Get the full set of budget predictions for the origin.
  void GetBudgetDetails() {
    base::RunLoop run_loop;
    db_.GetBudgetDetails(
        GURL(kTestOrigin),
        base::Bind(&BudgetDatabaseTest::GetBudgetDetailsComplete,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  Profile* profile() { return &profile_; }

  // Setup a test clock so that the tests can control time.
  base::SimpleTestClock* SetClockForTesting() {
    base::SimpleTestClock* clock = new base::SimpleTestClock();
    db_.SetClockForTesting(base::WrapUnique(clock));
    return clock;
  }

  // Query the database to check if the origin is in the cache.
  bool IsCached(const GURL& origin) { return db_.IsCached(origin); }

 protected:
  bool success_;
  std::vector<BudgetDatabase::BudgetStatus> prediction_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<budget_service::Budget> budget_;
  TestingProfile profile_;
  BudgetDatabase db_;
};

TEST_F(BudgetDatabaseTest, ReadAndWriteTest) {
  const GURL origin(kTestOrigin);
  base::SimpleTestClock* clock = SetClockForTesting();
  base::TimeDelta expiration(
      base::TimeDelta::FromHours(kDefaultExpirationInHours));
  base::Time starting_time = clock->Now();
  base::Time expiration_time = clock->Now() + expiration;

  // Add two budget chunks with different expirations (default expiration and
  // default expiration + 1 day).
  ASSERT_TRUE(AddBudget(origin, kDefaultBudget1));
  clock->Advance(base::TimeDelta::FromDays(1));
  ASSERT_TRUE(AddBudget(origin, kDefaultBudget2));

  // Get the budget.
  GetBudgetDetails();

  // Get the prediction and validate it.
  ASSERT_TRUE(success_);
  ASSERT_EQ(3U, prediction_.size());

  // Make sure that the correct data is returned.
  // First value should be [total_budget, now]
  EXPECT_EQ(kDefaultBudget1 + kDefaultBudget2, prediction_[0].budget_at);
  EXPECT_EQ(clock->Now(), prediction_[0].time);

  // The next value should be the budget after the first chunk expires.
  EXPECT_EQ(kDefaultBudget2, prediction_[1].budget_at);
  EXPECT_EQ(expiration_time, prediction_[1].time);

  // The final value gives the budget of 0.0 after the second chunk expires.
  expiration_time += base::TimeDelta::FromDays(1);
  EXPECT_EQ(0, prediction_[2].budget_at);
  EXPECT_EQ(expiration_time, prediction_[2].time);

  // Advance the time until the first chunk of budget should be expired.
  clock->SetNow(starting_time +
                base::TimeDelta::FromHours(kDefaultExpirationInHours));

  // Get the new budget and check that kDefaultBudget1 has been removed.
  GetBudgetDetails();
  ASSERT_EQ(2U, prediction_.size());
  EXPECT_EQ(kDefaultBudget2, prediction_[0].budget_at);
  EXPECT_EQ(0, prediction_[1].budget_at);

  // Advace the time until both chunks of budget should be expired.
  clock->SetNow(starting_time +
                base::TimeDelta::FromHours(kDefaultExpirationInHours) +
                base::TimeDelta::FromDays(1));

  GetBudgetDetails();
  ASSERT_EQ(1U, prediction_.size());
  EXPECT_EQ(0, prediction_[0].budget_at);

  // Now that the entire budget has expired, check that the entry in the map
  // has been removed.
  EXPECT_FALSE(IsCached(origin));
}

TEST_F(BudgetDatabaseTest, AddEngagementBudgetTest) {
  const GURL origin(kTestOrigin);
  base::SimpleTestClock* clock = SetClockForTesting();
  base::Time expiration_time =
      clock->Now() + base::TimeDelta::FromHours(kDefaultExpirationInHours);

  // Add a chunk of budget to a non-existant origin. This should add the full
  // amount of engagement.
  ASSERT_TRUE(AddEngagementBudget(origin, kDefaultEngagement));

  // The budget should include a full share of the engagement.
  GetBudgetDetails();
  ASSERT_TRUE(success_);
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_EQ(kDefaultEngagement, prediction_[0].budget_at);
  ASSERT_EQ(0, prediction_[1].budget_at);
  ASSERT_EQ(expiration_time, prediction_[1].time);

  // Advance time 1 day and add more engagement budget.
  clock->Advance(base::TimeDelta::FromDays(1));
  ASSERT_TRUE(AddEngagementBudget(origin, kDefaultEngagement));

  // The budget should now have 1 full share plus 1/3 share.
  GetBudgetDetails();
  ASSERT_TRUE(success_);
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultEngagement * 4 / 3, prediction_[0].budget_at);
  ASSERT_DOUBLE_EQ(kDefaultEngagement * 1 / 3, prediction_[1].budget_at);
  ASSERT_EQ(expiration_time, prediction_[1].time);
  ASSERT_EQ(0, prediction_[2].budget_at);
  ASSERT_EQ(expiration_time + base::TimeDelta::FromDays(1),
            prediction_[2].time);

  // Advance time by 59 minutes and check that no engagement budget is added
  // since budget should only be added for > 1 hour increments.
  clock->Advance(base::TimeDelta::FromMinutes(59));
  ASSERT_TRUE(AddEngagementBudget(origin, kDefaultEngagement));

  // The budget should be the same as before the attempted add.
  GetBudgetDetails();
  ASSERT_TRUE(success_);
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultEngagement * 4 / 3, prediction_[0].budget_at);
}

TEST_F(BudgetDatabaseTest, SpendBudgetTest) {
  const GURL origin(kTestOrigin);
  base::SimpleTestClock* clock = SetClockForTesting();
  base::Time starting_time = clock->Now();

  // Intialize the budget with several chunks.
  ASSERT_TRUE(AddBudget(origin, kDefaultBudget1));
  clock->Advance(base::TimeDelta::FromDays(1));
  ASSERT_TRUE(AddBudget(origin, kDefaultBudget1));
  clock->Advance(base::TimeDelta::FromDays(1));
  ASSERT_TRUE(AddBudget(origin, kDefaultBudget1));

  // Reset the clock then spend an amount of budget less than kDefaultBudget.
  clock->SetNow(starting_time);
  ASSERT_TRUE(SpendBudget(origin, 1));
  GetBudgetDetails();

  // There should still be three chunks of budget of size kDefaultBudget-1,
  // kDefaultBudget, and kDefaultBudget.
  ASSERT_EQ(4U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultBudget1 * 3 - 1, prediction_[0].budget_at);
  ASSERT_DOUBLE_EQ(kDefaultBudget1 * 2, prediction_[1].budget_at);
  ASSERT_DOUBLE_EQ(kDefaultBudget1, prediction_[2].budget_at);
  ASSERT_DOUBLE_EQ(0, prediction_[3].budget_at);

  // Now spend enough that it will use up the rest of the first chunk and all of
  // the second chunk, but not all of the third chunk.
  ASSERT_TRUE(SpendBudget(origin, kDefaultBudget1 * 2));
  GetBudgetDetails();
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultBudget1 - 1, prediction_.begin()->budget_at);

  // Validate that the code returns false if SpendBudget tries to spend more
  // budget than the origin has.
  EXPECT_FALSE(SpendBudget(origin, kDefaultBudget1));
  GetBudgetDetails();
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultBudget1 - 1, prediction_.begin()->budget_at);

  // Advance time until the last remaining chunk should be expired, then query
  // for what would be a valid amount of budget if the chunks weren't expired.
  clock->Advance(base::TimeDelta::FromDays(6));
  EXPECT_FALSE(SpendBudget(origin, 0.01));
}
