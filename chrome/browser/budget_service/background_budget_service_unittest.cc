// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/budget_service/background_budget_service.h"
#include "chrome/browser/budget_service/background_budget_service_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestOrigin[] = "https://example.com";
const double kTestBudget = 10.0;
const double kTestSES = 48.0;
const double kLowSES = 1.0;
const double kMaxSES = 100.0;
// Mirrors definition in BackgroundBudgetService, this is 10 days of seconds.
const double kSecondsToAccumulate = 864000.0;

}  // namespace

class BackgroundBudgetServiceTest : public testing::Test {
 public:
  BackgroundBudgetServiceTest() : budget_(0.0) {}
  ~BackgroundBudgetServiceTest() override {}

  BackgroundBudgetService* GetService() {
    return BackgroundBudgetServiceFactory::GetForProfile(&profile_);
  }

  void SetSiteEngagementScore(const GURL& url, double score) {
    SiteEngagementService* service = SiteEngagementService::Get(&profile_);
    service->ResetScoreForURL(url, score);
  }

  Profile* profile() { return &profile_; }

  base::SimpleTestClock* SetClockForTesting() {
    base::SimpleTestClock* clock = new base::SimpleTestClock();
    BackgroundBudgetServiceFactory::GetForProfile(&profile_)
        ->SetClockForTesting(base::WrapUnique(clock));
    return clock;
  }

  double GetBudget() {
    const GURL origin(kTestOrigin);
    base::RunLoop run_loop;
    GetService()->GetBudget(
        origin, base::Bind(&BackgroundBudgetServiceTest::GotBudget,
                           base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return budget_;
  }

  void GotBudget(base::Closure run_loop_closure, double budget) {
    budget_ = budget;
    run_loop_closure.Run();
  }

  void StoreBudget(double budget) {
    const GURL origin(kTestOrigin);
    base::RunLoop run_loop;
    GetService()->StoreBudget(origin, budget, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Budget for callbacks to set.
  double budget_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

TEST_F(BackgroundBudgetServiceTest, GetBudgetNoBudgetOrSES) {
  EXPECT_DOUBLE_EQ(GetBudget(), 0.0);
}

TEST_F(BackgroundBudgetServiceTest, GetBudgetNoBudgetSESExists) {
  // Set a starting SES for the url but no stored budget info.
  const GURL origin(kTestOrigin);
  SetSiteEngagementScore(origin, kTestSES);

  EXPECT_DOUBLE_EQ(GetBudget(), kTestSES);
}

TEST_F(BackgroundBudgetServiceTest, GetBudgetNoElapsedTime) {
  StoreBudget(kTestBudget);
  EXPECT_DOUBLE_EQ(GetBudget(), kTestBudget);
}

TEST_F(BackgroundBudgetServiceTest, GetBudgetElapsedTime) {
  // Manually construct a BackgroundBudgetServie with a clock that the test
  // can control so that we can fast forward in time.
  base::SimpleTestClock* clock = SetClockForTesting();
  base::Time starting_time = clock->Now();

  // Set initial SES and budget values.
  const GURL origin(kTestOrigin);
  SetSiteEngagementScore(origin, kTestSES);
  StoreBudget(kTestBudget);

  double budget = GetBudget();
  EXPECT_DOUBLE_EQ(budget, kTestBudget);

  // Query for the budget after 1 second has passed.
  clock->SetNow(starting_time + base::TimeDelta::FromSeconds(1));
  budget = GetBudget();
  EXPECT_LT(budget, kTestBudget + kTestSES * 1.0 / kSecondsToAccumulate);
  EXPECT_GT(budget, kTestBudget);

  // Query for the budget after 1 hour has passed.
  clock->SetNow(starting_time + base::TimeDelta::FromHours(1));
  budget = GetBudget();
  EXPECT_LT(budget, kTestBudget + kTestSES * 3600.0 / kSecondsToAccumulate);
  EXPECT_GT(budget, kTestBudget);

  // Query for the budget after 5 days have passed. The budget should be
  // increasing, but not up the SES score.
  clock->SetNow(starting_time + base::TimeDelta::FromDays(5));
  budget = GetBudget();
  EXPECT_GT(budget, kTestBudget);
  EXPECT_LT(budget, kTestSES);
  double moderate_ses_budget = budget;

  // Query for the budget after 10 days have passed. By this point, the budget
  // should converge to the SES score.
  clock->SetNow(starting_time + base::TimeDelta::FromDays(10));
  budget = GetBudget();
  EXPECT_DOUBLE_EQ(budget, kTestSES);

  // Now, change the SES score to the maximum amount and reinitialize budget.
  SetSiteEngagementScore(origin, kMaxSES);
  StoreBudget(kTestBudget);
  starting_time = clock->Now();

  // Query for the budget after 1 second has passed.
  clock->SetNow(starting_time + base::TimeDelta::FromSeconds(1));
  budget = GetBudget();
  EXPECT_LT(budget, kTestBudget + kMaxSES * 1.0 / kSecondsToAccumulate);

  // Query for the budget after 5 days have passed. Again, the budget should be
  // approaching the SES, but not have reached it.
  clock->SetNow(starting_time + base::TimeDelta::FromDays(5));
  budget = GetBudget();
  EXPECT_GT(budget, kTestBudget);
  EXPECT_LT(budget, kMaxSES);

  // The budget after 5 days with max SES should be greater than the budget
  // after 5 days with moderate SES.
  EXPECT_GT(budget, moderate_ses_budget);

  // Now, change the SES score to a low amount and reinitialize budget.
  SetSiteEngagementScore(origin, kLowSES);
  StoreBudget(kTestBudget);
  starting_time = clock->Now();

  // Query for the budget after 5 days have passed. Again, the budget should be
  // approaching the SES, this time decreasing, but not have reached it.
  clock->SetNow(starting_time + base::TimeDelta::FromDays(5));
  budget = GetBudget();
  EXPECT_LT(budget, kTestBudget);
  EXPECT_GT(budget, kLowSES);
}

TEST_F(BackgroundBudgetServiceTest, GetBudgetConsumedOverTime) {
  // Manually construct a BackgroundBudgetService with a clock that the test
  // can control so that we can fast forward in time.
  base::SimpleTestClock* clock = SetClockForTesting();

  // Set initial SES and budget values.
  const GURL origin(kTestOrigin);
  SetSiteEngagementScore(origin, kTestSES);
  StoreBudget(kTestBudget);
  double budget = 0.0;

  // Measure over 200 hours. In each hour a message is received, and for 1 in
  // 10, budget is consumed.
  for (int i = 0; i < 200; i++) {
    // Query for the budget after 1 hour has passed.
    clock->Advance(base::TimeDelta::FromHours(1));
    budget = GetBudget();

    if (i % 10 == 0) {
      double cost = BackgroundBudgetService::GetCost(
          BackgroundBudgetService::CostType::SILENT_PUSH);
      StoreBudget(budget - cost);
    }
  }

  // With a SES of 48.0, the origin will get a budget of 2.4 per day, but the
  // old budget will also decay. At the end, we expect the budget to be lower
  // than the starting budget.
  EXPECT_GT(budget, 0.0);
  EXPECT_LT(budget, kTestBudget);
}

TEST_F(BackgroundBudgetServiceTest, GetBudgetInvalidBudget) {
  const GURL origin(kTestOrigin);

  // Set a starting SES for the url.
  SetSiteEngagementScore(origin, kTestSES);

  // Set a badly formatted budget in the user preferences.
  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kBackgroundBudgetMap);
  base::DictionaryValue* update_map = update.Get();
  update_map->SetStringWithoutPathExpansion(origin.spec(), "20#2.0");

  // Get the budget, expect that it will return SES.
  EXPECT_DOUBLE_EQ(GetBudget(), kTestSES);
}

TEST_F(BackgroundBudgetServiceTest, GetBudgetNegativeTime) {
  // Manually construct a BackgroundBudgetService with a clock that the test
  // can control so that we can fast forward in time.
  base::SimpleTestClock* clock = SetClockForTesting();
  base::Time starting_time = clock->Now();

  // Set initial SES and budget values.
  const GURL origin(kTestOrigin);
  SetSiteEngagementScore(origin, kTestSES);
  StoreBudget(kTestBudget);

  // Move time forward an hour and get the budget.
  clock->SetNow(starting_time + base::TimeDelta::FromHours(1));
  double original_budget = GetBudget();

  // Store the updated budget.
  StoreBudget(original_budget);
  EXPECT_NE(kTestBudget, original_budget);

  // Now move time backwards a day and make sure that the current
  // budget matches the budget of the most foward time.
  clock->SetNow(starting_time - base::TimeDelta::FromDays(1));
  EXPECT_NEAR(original_budget, GetBudget(), 0.01);
}
