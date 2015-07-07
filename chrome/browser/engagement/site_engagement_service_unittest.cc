// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kLessNavigationsThanNeededToMaxDailyEngagement = 2;
const int kMoreNavigationsThanNeededToMaxDailyEngagement = 20;
const int kMoreNavigationsThanNeededToMaxTotalEngagement = 200;
const int kLessDaysThanNeededToMaxTotalEngagement = 4;
const int kMoreDaysThanNeededToMaxTotalEngagement = 40;
const int kLessPeriodsThanNeededToDecayMaxScore = 2;
const int kMorePeriodsThanNeededToDecayMaxScore = 40;

base::Time GetReferenceTime() {
  base::Time::Exploded exploded_reference_time;
  exploded_reference_time.year = 2015;
  exploded_reference_time.month = 1;
  exploded_reference_time.day_of_month = 30;
  exploded_reference_time.day_of_week = 5;
  exploded_reference_time.hour = 11;
  exploded_reference_time.minute = 0;
  exploded_reference_time.second = 0;
  exploded_reference_time.millisecond = 0;

  return base::Time::FromLocalExploded(exploded_reference_time);
}

}  // namespace

class SiteEngagementScoreTest : public testing::Test {
 public:
  SiteEngagementScoreTest() : score_(&test_clock_) {}

 protected:
  void VerifyScore(const SiteEngagementScore& score,
                   double expected_raw_score,
                   double expected_points_added_today,
                   base::Time expected_last_engagement_time) {
    EXPECT_EQ(expected_raw_score, score.raw_score_);
    EXPECT_EQ(expected_points_added_today, score.points_added_today_);
    EXPECT_EQ(expected_last_engagement_time, score.last_engagement_time_);
  }

  void UpdateScore(SiteEngagementScore* score,
                   double raw_score,
                   double points_added_today,
                   base::Time last_engagement_time) {
    score->raw_score_ = raw_score;
    score->points_added_today_ = points_added_today;
    score->last_engagement_time_ = last_engagement_time;
  }

  void TestScoreInitializesAndUpdates(
      base::DictionaryValue* score_dict,
      double expected_raw_score,
      double expected_points_added_today,
      base::Time expected_last_engagement_time) {
    SiteEngagementScore initial_score(&test_clock_, *score_dict);
    VerifyScore(initial_score, expected_raw_score, expected_points_added_today,
                expected_last_engagement_time);

    // Updating the score dict should return false, as the score shouldn't
    // have changed at this point.
    EXPECT_FALSE(initial_score.UpdateScoreDict(score_dict));

    // Update the score to new values and verify it updates the score dict
    // correctly.
    base::Time different_day =
        GetReferenceTime() + base::TimeDelta::FromDays(1);
    UpdateScore(&initial_score, 5, 10, different_day);
    EXPECT_TRUE(initial_score.UpdateScoreDict(score_dict));
    SiteEngagementScore updated_score(&test_clock_, *score_dict);
    VerifyScore(updated_score, 5, 10, different_day);
  }

  base::SimpleTestClock test_clock_;
  SiteEngagementScore score_;
};

// Navigate many times on the same day. Ensure each time the score goes up by
// kNavigationPoints, but not more than kMaxPointsPerDay.
TEST_F(SiteEngagementScoreTest, NavigateOnSameDay) {
  base::Time reference_time = GetReferenceTime();

  test_clock_.SetNow(reference_time);
  for (int i = 0; i < kMoreNavigationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::kNavigationPoints);
    EXPECT_EQ(std::min(SiteEngagementScore::kMaxPointsPerDay,
                       (i + 1) * SiteEngagementScore::kNavigationPoints),
              score_.Score());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPointsPerDay, score_.Score());
}

// Navigate on the first day to max that day's engagement, then navigate on a
// different day.
TEST_F(SiteEngagementScoreTest, NavigateOnTwoDays) {
  base::Time reference_time = GetReferenceTime();
  base::Time later_date = reference_time + base::TimeDelta::FromDays(2);

  test_clock_.SetNow(reference_time);
  for (int i = 0; i < kMoreNavigationsThanNeededToMaxDailyEngagement; ++i)
    score_.AddPoints(SiteEngagementScore::kNavigationPoints);

  EXPECT_EQ(SiteEngagementScore::kMaxPointsPerDay, score_.Score());

  test_clock_.SetNow(later_date);
  for (int i = 0; i < kMoreNavigationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::kNavigationPoints);
    double day_score =
        std::min(SiteEngagementScore::kMaxPointsPerDay,
                 (i + 1) * SiteEngagementScore::kNavigationPoints);
    EXPECT_EQ(day_score + SiteEngagementScore::kMaxPointsPerDay,
              score_.Score());
  }

  EXPECT_EQ(2 * SiteEngagementScore::kMaxPointsPerDay, score_.Score());
}

// Navigate a lot on many consecutive days and ensure the score doesn't exceed
// the maximum allowed.
TEST_F(SiteEngagementScoreTest, NavigateALotOnManyDays) {
  base::Time current_day = GetReferenceTime();

  for (int i = 0; i < kMoreDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    test_clock_.SetNow(current_day);
    for (int j = 0; j < kMoreNavigationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::kNavigationPoints);

    EXPECT_EQ(std::min(SiteEngagementScore::kMaxPoints,
                       (i + 1) * SiteEngagementScore::kMaxPointsPerDay),
              score_.Score());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());
}

// Navigate a little on many consecutive days and ensure the score doesn't
// exceed the maximum allowed.
TEST_F(SiteEngagementScoreTest, NavigateALittleOnManyDays) {
  base::Time current_day = GetReferenceTime();

  for (int i = 0; i < kMoreNavigationsThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    test_clock_.SetNow(current_day);

    for (int j = 0; j < kLessNavigationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::kNavigationPoints);

    EXPECT_EQ(
        std::min(SiteEngagementScore::kMaxPoints,
                 (i + 1) * kLessNavigationsThanNeededToMaxDailyEngagement *
                     SiteEngagementScore::kNavigationPoints),
        score_.Score());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());
}

// Navigate a bit, then check the score decays properly for a range of times.
TEST_F(SiteEngagementScoreTest, ScoresDecayOverTime) {
  base::Time current_day = GetReferenceTime();

  // First max the score.
  for (int i = 0; i < kMoreDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    test_clock_.SetNow(current_day);

    for (int j = 0; j < kMoreNavigationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::kNavigationPoints);
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());

  // The score should not have decayed before the first decay period has
  // elapsed.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(SiteEngagementScore::kDecayPeriodInDays - 1));
  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());

  // The score should have decayed by one chunk after one decay period has
  // elapsed.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(SiteEngagementScore::kDecayPeriodInDays));
  EXPECT_EQ(SiteEngagementScore::kMaxPoints - SiteEngagementScore::kDecayPoints,
            score_.Score());

  // The score should have decayed by the right number of chunks after a few
  // decay periods have elapsed.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(kLessPeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::kDecayPeriodInDays));
  EXPECT_EQ(SiteEngagementScore::kMaxPoints -
                kLessPeriodsThanNeededToDecayMaxScore *
                    SiteEngagementScore::kDecayPoints,
            score_.Score());

  // The score should not decay below zero.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(kMorePeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::kDecayPeriodInDays));
  EXPECT_EQ(0, score_.Score());
}

// Test that any expected decays are applied before adding points.
TEST_F(SiteEngagementScoreTest, DecaysAppliedBeforeAdd) {
  base::Time current_day = GetReferenceTime();

  // Get the score up to something that can handle a bit of decay before
  for (int i = 0; i < kLessDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    test_clock_.SetNow(current_day);

    for (int j = 0; j < kMoreNavigationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::kNavigationPoints);
  }

  double initial_score = kLessDaysThanNeededToMaxTotalEngagement *
                         SiteEngagementScore::kMaxPointsPerDay;
  EXPECT_EQ(initial_score, score_.Score());

  // Go forward a few decay periods.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(kLessPeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::kDecayPeriodInDays));

  double decayed_score =
      initial_score -
      kLessPeriodsThanNeededToDecayMaxScore * SiteEngagementScore::kDecayPoints;
  EXPECT_EQ(decayed_score, score_.Score());

  // Now add some points.
  score_.AddPoints(SiteEngagementScore::kNavigationPoints);
  EXPECT_EQ(decayed_score + SiteEngagementScore::kNavigationPoints,
            score_.Score());
}

// Test that going back in time is handled properly.
TEST_F(SiteEngagementScoreTest, GoBackInTime) {
  base::Time current_day = GetReferenceTime();

  test_clock_.SetNow(current_day);
  for (int i = 0; i < kMoreNavigationsThanNeededToMaxDailyEngagement; ++i)
    score_.AddPoints(SiteEngagementScore::kNavigationPoints);

  EXPECT_EQ(SiteEngagementScore::kMaxPointsPerDay, score_.Score());

  // Adding to the score on an earlier date should be treated like another day,
  // and should not cause any decay.
  test_clock_.SetNow(current_day - base::TimeDelta::FromDays(
                                       kMorePeriodsThanNeededToDecayMaxScore *
                                       SiteEngagementScore::kDecayPoints));
  for (int i = 0; i < kMoreNavigationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::kNavigationPoints);
    double day_score =
        std::min(SiteEngagementScore::kMaxPointsPerDay,
                 (i + 1) * SiteEngagementScore::kNavigationPoints);
    EXPECT_EQ(day_score + SiteEngagementScore::kMaxPointsPerDay,
              score_.Score());
  }

  EXPECT_EQ(2 * SiteEngagementScore::kMaxPointsPerDay, score_.Score());
}

// Test that scores are read / written correctly from / to empty score
// dictionaries.
TEST_F(SiteEngagementScoreTest, EmptyDictionary) {
  base::DictionaryValue dict;
  TestScoreInitializesAndUpdates(&dict, 0, 0, base::Time());
}

// Test that scores are read / written correctly from / to partially empty
// score dictionaries.
TEST_F(SiteEngagementScoreTest, PartiallyEmptyDictionary) {
  base::DictionaryValue dict;
  dict.SetDouble(SiteEngagementScore::kPointsAddedTodayKey, 2);

  TestScoreInitializesAndUpdates(&dict, 0, 2, base::Time());
}

// Test that scores are read / written correctly from / to populated score
// dictionaries.
TEST_F(SiteEngagementScoreTest, PopulatedDictionary) {
  base::DictionaryValue dict;
  dict.SetDouble(SiteEngagementScore::kRawScoreKey, 1);
  dict.SetDouble(SiteEngagementScore::kPointsAddedTodayKey, 2);
  dict.SetDouble(SiteEngagementScore::kLastEngagementTimeKey,
                  GetReferenceTime().ToInternalValue());

  TestScoreInitializesAndUpdates(&dict, 1, 2, GetReferenceTime());
}


class SiteEngagementServiceTest : public BrowserWithTestWindowTest {
 public:
  SiteEngagementServiceTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableSiteEngagementService);
  }
};


// Tests that the Site Engagement service is hooked up properly to navigations
// by performing two navigations and checking the engagement score increases
// both times.
TEST_F(SiteEngagementServiceTest, ScoreIncrementsOnPageRequest) {
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile());
  DCHECK(service);

  GURL url("http://www.google.com/");

  AddTab(browser(), GURL("about:blank"));
  EXPECT_EQ(0, service->GetScore(url));
  int prev_score = service->GetScore(url);

  NavigateAndCommitActiveTab(url);
  EXPECT_LT(prev_score, service->GetScore(url));
  prev_score = service->GetScore(url);

  NavigateAndCommitActiveTab(url);
  EXPECT_LT(prev_score, service->GetScore(url));
}

// Expect that site engagement scores for several sites are correctly aggregated
// by GetTotalEngagementPoints().
TEST_F(SiteEngagementServiceTest, GetTotalEngagementPoints) {
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile());
  DCHECK(service);

  // The https and http versions of www.google.com should be separate.
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  GURL url3("http://drive.google.com/");

  EXPECT_EQ(0, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_EQ(0, service->GetScore(url3));

  service->HandleNavigation(url1);
  EXPECT_EQ(1, service->GetScore(url1));
  EXPECT_EQ(1, service->GetTotalEngagementPoints());

  service->HandleNavigation(url2);
  service->HandleNavigation(url2);
  EXPECT_EQ(2, service->GetScore(url2));
  EXPECT_EQ(3, service->GetTotalEngagementPoints());

  service->HandleNavigation(url3);
  EXPECT_EQ(1, service->GetScore(url3));
  EXPECT_EQ(4, service->GetTotalEngagementPoints());

  service->HandleNavigation(url1);
  service->HandleNavigation(url1);
  EXPECT_EQ(3, service->GetScore(url1));
  EXPECT_EQ(6, service->GetTotalEngagementPoints());
}
