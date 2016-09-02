// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_database.h"

#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/budget_service/budget.pb.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const double kDefaultExpirationInHours = 72;
const double kDefaultEngagement = 30.0;

const char kTestOrigin[] = "https://example.com";

}  // namespace

class BudgetDatabaseTest : public ::testing::Test {
 public:
  BudgetDatabaseTest()
      : success_(false),
        db_(&profile_,
            profile_.GetPath().Append(FILE_PATH_LITERAL("BudgetDatabase")),
            base::ThreadTaskRunnerHandle::Get()) {}

  void WriteBudgetComplete(base::Closure run_loop_closure, bool success) {
    success_ = success;
    run_loop_closure.Run();
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

  void SetSiteEngagementScore(const GURL& url, double score) {
    SiteEngagementService* service = SiteEngagementService::Get(&profile_);
    service->ResetScoreForURL(url, score);
  }

 protected:
  bool success_;
  std::vector<BudgetDatabase::BudgetStatus> prediction_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<budget_service::Budget> budget_;
  TestingProfile profile_;
  BudgetDatabase db_;
};

TEST_F(BudgetDatabaseTest, AddEngagementBudgetTest) {
  const GURL origin(kTestOrigin);
  base::SimpleTestClock* clock = SetClockForTesting();
  base::Time expiration_time =
      clock->Now() + base::TimeDelta::FromHours(kDefaultExpirationInHours);

  // Set the default site engagement.
  SetSiteEngagementScore(origin, kDefaultEngagement);

  // The budget should include a full share of the engagement.
  GetBudgetDetails();
  ASSERT_TRUE(success_);
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_EQ(kDefaultEngagement, prediction_[0].budget_at);
  ASSERT_EQ(0, prediction_[1].budget_at);
  ASSERT_EQ(expiration_time, prediction_[1].time);

  // Advance time 1 day and add more engagement budget.
  clock->Advance(base::TimeDelta::FromDays(1));
  GetBudgetDetails();

  // The budget should now have 1 full share plus 1/3 share.
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

  // The budget should be the same as before the attempted add.
  GetBudgetDetails();
  ASSERT_TRUE(success_);
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultEngagement * 4 / 3, prediction_[0].budget_at);
}

TEST_F(BudgetDatabaseTest, SpendBudgetTest) {
  const GURL origin(kTestOrigin);
  base::SimpleTestClock* clock = SetClockForTesting();

  // Set the default site engagement.
  SetSiteEngagementScore(origin, kDefaultEngagement);

  // Intialize the budget with several chunks.
  GetBudgetDetails();
  clock->Advance(base::TimeDelta::FromDays(1));
  GetBudgetDetails();
  clock->Advance(base::TimeDelta::FromDays(1));
  GetBudgetDetails();

  // Spend an amount of budget less than kDefaultEngagement.
  ASSERT_TRUE(SpendBudget(origin, 1));
  GetBudgetDetails();

  // There should still be three chunks of budget of size kDefaultEngagement-1,
  // kDefaultEngagement, and kDefaultEngagement.
  ASSERT_EQ(4U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultEngagement * 5 / 3 - 1, prediction_[0].budget_at);
  ASSERT_DOUBLE_EQ(kDefaultEngagement * 2 / 3, prediction_[1].budget_at);
  ASSERT_DOUBLE_EQ(kDefaultEngagement * 1 / 3, prediction_[2].budget_at);
  ASSERT_DOUBLE_EQ(0, prediction_[3].budget_at);

  // Now spend enough that it will use up the rest of the first chunk and all of
  // the second chunk, but not all of the third chunk.
  ASSERT_TRUE(SpendBudget(origin, kDefaultEngagement * 4 / 3));
  GetBudgetDetails();
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultEngagement * 1 / 3 - 1,
                   prediction_.begin()->budget_at);

  // Validate that the code returns false if SpendBudget tries to spend more
  // budget than the origin has.
  EXPECT_FALSE(SpendBudget(origin, kDefaultEngagement));
  GetBudgetDetails();
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultEngagement * 1 / 3 - 1,
                   prediction_.begin()->budget_at);

  // Advance time until the last remaining chunk should be expired, then query
  // for the full engagement worth of budget.
  clock->Advance(base::TimeDelta::FromDays(6));
  EXPECT_TRUE(SpendBudget(origin, kDefaultEngagement));
}
