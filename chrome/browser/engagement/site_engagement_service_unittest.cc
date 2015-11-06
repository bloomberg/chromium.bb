// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/page_navigator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kLessAccumulationsThanNeededToMaxDailyEngagement = 2;
const int kMoreAccumulationsThanNeededToMaxDailyEngagement = 40;
const int kMoreAccumulationsThanNeededToMaxTotalEngagement = 200;
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

// Accumulate score many times on the same day. Ensure each time the score goes
// up by g_navigation_points, but not more than g_max_points_per_day.
TEST_F(SiteEngagementScoreTest, AccumulateOnSameDay) {
  base::Time reference_time = GetReferenceTime();

  test_clock_.SetNow(reference_time);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::g_navigation_points);
    EXPECT_EQ(std::min(SiteEngagementScore::g_max_points_per_day,
                       (i + 1) * SiteEngagementScore::g_navigation_points),
              score_.Score());
  }

  EXPECT_EQ(SiteEngagementScore::g_max_points_per_day, score_.Score());
}

// Accumulate on the first day to max that day's engagement, then accumulate on
// a different day.
TEST_F(SiteEngagementScoreTest, AccumulateOnTwoDays) {
  base::Time reference_time = GetReferenceTime();
  base::Time later_date = reference_time + base::TimeDelta::FromDays(2);

  test_clock_.SetNow(reference_time);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i)
    score_.AddPoints(SiteEngagementScore::g_navigation_points);

  EXPECT_EQ(SiteEngagementScore::g_max_points_per_day, score_.Score());

  test_clock_.SetNow(later_date);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::g_navigation_points);
    double day_score =
        std::min(SiteEngagementScore::g_max_points_per_day,
                 (i + 1) * SiteEngagementScore::g_navigation_points);
    EXPECT_EQ(day_score + SiteEngagementScore::g_max_points_per_day,
              score_.Score());
  }

  EXPECT_EQ(2 * SiteEngagementScore::g_max_points_per_day, score_.Score());
}

// Accumulate score on many consecutive days and ensure the score doesn't exceed
// the maximum allowed.
TEST_F(SiteEngagementScoreTest, AccumulateALotOnManyDays) {
  base::Time current_day = GetReferenceTime();

  for (int i = 0; i < kMoreDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    test_clock_.SetNow(current_day);
    for (int j = 0; j < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::g_navigation_points);

    EXPECT_EQ(std::min(SiteEngagementScore::kMaxPoints,
                       (i + 1) * SiteEngagementScore::g_max_points_per_day),
              score_.Score());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());
}

// Accumulate a little on many consecutive days and ensure the score doesn't
// exceed the maximum allowed.
TEST_F(SiteEngagementScoreTest, AccumulateALittleOnManyDays) {
  base::Time current_day = GetReferenceTime();

  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    test_clock_.SetNow(current_day);

    for (int j = 0; j < kLessAccumulationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::g_navigation_points);

    EXPECT_EQ(
        std::min(SiteEngagementScore::kMaxPoints,
                 (i + 1) * kLessAccumulationsThanNeededToMaxDailyEngagement *
                     SiteEngagementScore::g_navigation_points),
        score_.Score());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());
}

// Accumulate a bit, then check the score decays properly for a range of times.
TEST_F(SiteEngagementScoreTest, ScoresDecayOverTime) {
  base::Time current_day = GetReferenceTime();

  // First max the score.
  for (int i = 0; i < kMoreDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    test_clock_.SetNow(current_day);

    for (int j = 0; j < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::g_navigation_points);
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());

  // The score should not have decayed before the first decay period has
  // elapsed.
  test_clock_.SetNow(current_day +
                     base::TimeDelta::FromDays(
                         SiteEngagementScore::g_decay_period_in_days - 1));
  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());

  // The score should have decayed by one chunk after one decay period has
  // elapsed.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(SiteEngagementScore::g_decay_period_in_days));
  EXPECT_EQ(
      SiteEngagementScore::kMaxPoints - SiteEngagementScore::g_decay_points,
      score_.Score());

  // The score should have decayed by the right number of chunks after a few
  // decay periods have elapsed.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(kLessPeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::g_decay_period_in_days));
  EXPECT_EQ(SiteEngagementScore::kMaxPoints -
                kLessPeriodsThanNeededToDecayMaxScore *
                    SiteEngagementScore::g_decay_points,
            score_.Score());

  // The score should not decay below zero.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(kMorePeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::g_decay_period_in_days));
  EXPECT_EQ(0, score_.Score());
}

// Test that any expected decays are applied before adding points.
TEST_F(SiteEngagementScoreTest, DecaysAppliedBeforeAdd) {
  base::Time current_day = GetReferenceTime();

  // Get the score up to something that can handle a bit of decay before
  for (int i = 0; i < kLessDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    test_clock_.SetNow(current_day);

    for (int j = 0; j < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::g_navigation_points);
  }

  double initial_score = kLessDaysThanNeededToMaxTotalEngagement *
                         SiteEngagementScore::g_max_points_per_day;
  EXPECT_EQ(initial_score, score_.Score());

  // Go forward a few decay periods.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(kLessPeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::g_decay_period_in_days));

  double decayed_score = initial_score -
                         kLessPeriodsThanNeededToDecayMaxScore *
                             SiteEngagementScore::g_decay_points;
  EXPECT_EQ(decayed_score, score_.Score());

  // Now add some points.
  score_.AddPoints(SiteEngagementScore::g_navigation_points);
  EXPECT_EQ(decayed_score + SiteEngagementScore::g_navigation_points,
            score_.Score());
}

// Test that going back in time is handled properly.
TEST_F(SiteEngagementScoreTest, GoBackInTime) {
  base::Time current_day = GetReferenceTime();

  test_clock_.SetNow(current_day);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i)
    score_.AddPoints(SiteEngagementScore::g_navigation_points);

  EXPECT_EQ(SiteEngagementScore::g_max_points_per_day, score_.Score());

  // Adding to the score on an earlier date should be treated like another day,
  // and should not cause any decay.
  test_clock_.SetNow(current_day - base::TimeDelta::FromDays(
                                       kMorePeriodsThanNeededToDecayMaxScore *
                                       SiteEngagementScore::g_decay_points));
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::g_navigation_points);
    double day_score =
        std::min(SiteEngagementScore::g_max_points_per_day,
                 (i + 1) * SiteEngagementScore::g_navigation_points);
    EXPECT_EQ(day_score + SiteEngagementScore::g_max_points_per_day,
              score_.Score());
  }

  EXPECT_EQ(2 * SiteEngagementScore::g_max_points_per_day, score_.Score());
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

  void NavigateWithTransitionAndExpectHigherScore(
      SiteEngagementService* service,
      GURL& url,
      ui::PageTransition transition) {
    double prev_score = service->GetScore(url);
    content::NavigationController* controller =
        &browser()->tab_strip_model()->GetActiveWebContents()->GetController();

    browser()->OpenURL(content::OpenURLParams(url, content::Referrer(),
                                              CURRENT_TAB, transition, false));
    CommitPendingLoad(controller);
    EXPECT_LT(prev_score, service->GetScore(url));
  }

  void NavigateWithTransitionAndExpectEqualScore(
      SiteEngagementService* service,
      GURL& url,
      ui::PageTransition transition) {
    double prev_score = service->GetScore(url);
    content::NavigationController* controller =
        &browser()->tab_strip_model()->GetActiveWebContents()->GetController();

    browser()->OpenURL(content::OpenURLParams(url, content::Referrer(),
                                              CURRENT_TAB, transition, false));
    CommitPendingLoad(controller);
    EXPECT_EQ(prev_score, service->GetScore(url));
  }
};

TEST_F(SiteEngagementServiceTest, GetMedianEngagement) {
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile());
  DCHECK(service);

  GURL url1("http://www.google.com/");
  GURL url2("https://www.google.com/");
  GURL url3("https://drive.google.com/");
  GURL url4("https://maps.google.com/");
  GURL url5("https://youtube.com/");
  GURL url6("https://images.google.com/");

  {
    // For zero total sites, the median is 0.
    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_TRUE(0 == score_map.size());
    EXPECT_DOUBLE_EQ(0, service->GetMedianEngagement(score_map));
  }

  {
    // For odd total sites, the median is the middle score.
    service->AddPoints(url1, 1);
    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_TRUE(1 == score_map.size());
    EXPECT_DOUBLE_EQ(1, service->GetMedianEngagement(score_map));
  }

  {
    // For even total sites, the median is the mean of the middle two scores.
    service->AddPoints(url2, 2);
    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_TRUE(2 == score_map.size());
    EXPECT_DOUBLE_EQ(1.5, service->GetMedianEngagement(score_map));
  }

  {
    service->AddPoints(url3, 1.4);
    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_TRUE(3 == score_map.size());
    EXPECT_DOUBLE_EQ(1.4, service->GetMedianEngagement(score_map));
  }

  {
    service->AddPoints(url4, 1.8);
    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_TRUE(4 == score_map.size());
    EXPECT_DOUBLE_EQ(1.6, service->GetMedianEngagement(score_map));
  }

  {
    service->AddPoints(url5, 2.5);
    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_TRUE(5 == score_map.size());
    EXPECT_DOUBLE_EQ(1.8, service->GetMedianEngagement(score_map));
  }

  {
    service->AddPoints(url6, 3);
    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_TRUE(6 == score_map.size());
    EXPECT_DOUBLE_EQ(1.9, service->GetMedianEngagement(score_map));
  }
}

// Tests that the Site Engagement service is hooked up properly to navigations
// by performing two navigations and checking the engagement score increases
// both times.
TEST_F(SiteEngagementServiceTest, ScoreIncrementsOnPageRequest) {
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile());
  DCHECK(service);

  GURL url("http://www.google.com/");
  EXPECT_EQ(0, service->GetScore(url));
  AddTab(browser(), GURL("about:blank"));

  NavigateWithTransitionAndExpectHigherScore(service, url,
                                             ui::PAGE_TRANSITION_TYPED);

  NavigateWithTransitionAndExpectHigherScore(service, url,
                                             ui::PAGE_TRANSITION_AUTO_BOOKMARK);
}

// Expect that site engagement scores for several sites are correctly
// aggregated during navigation events.
TEST_F(SiteEngagementServiceTest, GetTotalNavigationPoints) {
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

  service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0.5, service->GetTotalEngagementPoints());

  service->HandleNavigation(url2, ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(url2, ui::PAGE_TRANSITION_KEYWORD_GENERATED);
  EXPECT_EQ(1, service->GetScore(url2));
  EXPECT_EQ(1.5, service->GetTotalEngagementPoints());

  service->HandleNavigation(url2, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  EXPECT_EQ(1.5, service->GetScore(url2));
  EXPECT_EQ(2, service->GetTotalEngagementPoints());

  service->HandleNavigation(url3, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(0.5, service->GetScore(url3));
  EXPECT_EQ(2.5, service->GetTotalEngagementPoints());

  service->HandleNavigation(url1, ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(1.5, service->GetScore(url1));
  EXPECT_EQ(3.5, service->GetTotalEngagementPoints());
}

TEST_F(SiteEngagementServiceTest, GetTotalUserInputPoints) {
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

  service->HandleUserInput(url1, SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  EXPECT_DOUBLE_EQ(0.05, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.05, service->GetTotalEngagementPoints());

  service->HandleUserInput(url2, SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  service->HandleUserInput(url2, SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  EXPECT_DOUBLE_EQ(0.1, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(0.15, service->GetTotalEngagementPoints());

  service->HandleUserInput(url3, SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  EXPECT_DOUBLE_EQ(0.05, service->GetScore(url3));
  EXPECT_DOUBLE_EQ(0.2, service->GetTotalEngagementPoints());

  service->HandleUserInput(url1, SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  service->HandleUserInput(url1, SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  EXPECT_DOUBLE_EQ(0.15, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.3, service->GetTotalEngagementPoints());

  service->HandleUserInput(url2, SiteEngagementMetrics::ENGAGEMENT_WHEEL);
  service->HandleUserInput(url3,
                           SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE);
  EXPECT_DOUBLE_EQ(0.15, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(0.1, service->GetScore(url3));
  EXPECT_DOUBLE_EQ(0.4, service->GetTotalEngagementPoints());
}

TEST_F(SiteEngagementServiceTest, CheckHistograms) {
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  scoped_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), make_scoped_ptr(clock)));

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

  base::HistogramTester histograms;

  // Histograms should start empty as the testing SiteEngagementService
  // constructor does not record metrics.
  histograms.ExpectTotalCount(SiteEngagementMetrics::kTotalEngagementHistogram,
                              0);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kTotalOriginsHistogram, 0);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMeanEngagementHistogram,
                              0);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMedianEngagementHistogram,
                              0);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementScoreHistogram,
                              0);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram, 0);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram, 0);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram, 0);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              0);

  // The https and http versions of www.google.com should be separate.
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  GURL url3("http://drive.google.com/");

  service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);
  service->HandleUserInput(url1, SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  service->HandleUserInput(url1, SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  service->HandleMediaPlaying(url2, true);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kTotalEngagementHistogram,
                              1);
  histograms.ExpectUniqueSample(SiteEngagementMetrics::kTotalOriginsHistogram,
                                1, 1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMeanEngagementHistogram,
                              1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMedianEngagementHistogram,
                              1);
  // Recorded per origin.
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementScoreHistogram,
                              1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram, 0, 1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram, 0, 1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram, 0, 1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              4);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_HIDDEN,
                               1);

  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromMinutes(59));

  service->HandleNavigation(url2, ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(url2, ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              6);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_HIDDEN,
                               1);

  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromMinutes(60));

  service->HandleNavigation(url3, ui::PAGE_TRANSITION_TYPED);
  service->HandleUserInput(url2,
                           SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE);
  service->HandleMediaPlaying(url3, false);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kTotalEngagementHistogram,
                              2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 1,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 3,
                               1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMeanEngagementHistogram,
                              2);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMedianEngagementHistogram,
                              2);
  // Recorded per origin.
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementScoreHistogram,
                              4);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram, 0, 2);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram, 0, 2);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram, 0, 2);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              9);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 4);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_VISIBLE,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_HIDDEN,
                               1);

  service->HandleNavigation(url1, ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);
  service->HandleUserInput(url2, SiteEngagementMetrics::ENGAGEMENT_WHEEL);
  service->HandleUserInput(url1, SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  service->HandleUserInput(url3, SiteEngagementMetrics::ENGAGEMENT_MOUSE);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              14);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 6);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_WHEEL, 1);

  // Advance an origin to the max for a day and advance the clock an hour before
  // the last increment before max. Expect the histogram to be updated.
  for (int i = 0; i < 6; ++i)
    service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);

  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromMinutes(121));
  service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kTotalEngagementHistogram,
                              3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 1,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 3,
                               2);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMeanEngagementHistogram,
                              3);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMedianEngagementHistogram,
                              3);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementScoreHistogram,
                              7);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram, 0, 3);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram, 0, 2);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram, 1, 1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram, 0, 3);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              21);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION,
                               13);
}

// Expect that sites that have reached zero engagement are cleaned up.
TEST_F(SiteEngagementServiceTest, CleanupEngagementScores) {
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  scoped_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), make_scoped_ptr(clock)));

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

  // The https and http versions of www.google.com should be separate.
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");

  EXPECT_EQ(0, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Add the maximum number of points for one day.
  service->AddPoints(url1, 5.0);
  EXPECT_EQ(5.0, service->GetScore(url1));
  service->AddPoints(url2, 5.0);
  EXPECT_EQ(5.0, service->GetScore(url2));

  // Add more points by moving to another day.
  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromDays(1));

  service->AddPoints(url1, 5.0);
  EXPECT_EQ(10.0, service->GetScore(url1));

  {
    // Decay one origin to zero by advancing time and expect the engagement
    // score to be cleaned up. The other score was changed a day later so it
    // will not have decayed at all.
    clock->SetNow(
        GetReferenceTime() +
        base::TimeDelta::FromDays(SiteEngagementScore::g_decay_period_in_days));

    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_EQ(2u, score_map.size());
    EXPECT_EQ(10, score_map[url1]);
    EXPECT_EQ(0, score_map[url2]);

    service->CleanupEngagementScores();

    score_map = service->GetScoreMap();
    EXPECT_EQ(1u, score_map.size());
    EXPECT_EQ(10, score_map[url1]);
    EXPECT_EQ(0, service->GetScore(url2));
  }

  {
    // Decay the other origin to zero by advancing time and expect the
    // engagement score to be cleaned up.
    clock->SetNow(GetReferenceTime() +
                  base::TimeDelta::FromDays(
                      3 * SiteEngagementScore::g_decay_period_in_days));

    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_EQ(1u, score_map.size());
    EXPECT_EQ(0, score_map[url1]);

    service->CleanupEngagementScores();

    score_map = service->GetScoreMap();
    EXPECT_EQ(0u, score_map.size());
    EXPECT_EQ(0, service->GetScore(url1));
  }
}

// Expect that history can be cleared for given origins.
TEST_F(SiteEngagementServiceTest, ClearHistoryForURLs) {
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile());

  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  GURL url3("http://www.example.com/");

  EXPECT_EQ(0u, service->GetScoreMap().size());

  service->AddPoints(url1, 5.0);
  EXPECT_EQ(5.0, service->GetScore(url1));
  service->AddPoints(url2, 5.0);
  EXPECT_EQ(5.0, service->GetScore(url2));
  service->AddPoints(url3, 5.0);
  EXPECT_EQ(5.0, service->GetScore(url3));
  EXPECT_EQ(3u, service->GetScoreMap().size());

  // Delete 2 of the origins, leaving one left.
  std::set<GURL> origins_to_clear;
  origins_to_clear.insert(url1);
  origins_to_clear.insert(url3);

  SiteEngagementService::ClearHistoryForURLs(profile(), origins_to_clear);

  std::map<GURL, double> score_map = service->GetScoreMap();
  EXPECT_EQ(1u, score_map.size());
  EXPECT_DOUBLE_EQ(5.0, score_map[url2]);

  // Ensure nothing bad happens when URLs in the clear list are not in the site
  // engagement service.
  SiteEngagementService::ClearHistoryForURLs(profile(), origins_to_clear);

  score_map = service->GetScoreMap();
  EXPECT_EQ(1u, score_map.size());
  EXPECT_DOUBLE_EQ(5.0, score_map[url2]);
}

TEST_F(SiteEngagementServiceTest, NavigationAccumulation) {
  AddTab(browser(), GURL("about:blank"));
  GURL url("https://www.google.com/");

  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  // Only direct navigation should trigger engagement.
  NavigateWithTransitionAndExpectHigherScore(service, url,
                                             ui::PAGE_TRANSITION_TYPED);
  NavigateWithTransitionAndExpectHigherScore(service, url,
                                             ui::PAGE_TRANSITION_GENERATED);
  NavigateWithTransitionAndExpectHigherScore(service, url,
                                             ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  NavigateWithTransitionAndExpectHigherScore(
      service, url, ui::PAGE_TRANSITION_KEYWORD_GENERATED);

  // Other transition types should not accumulate engagement.
  NavigateWithTransitionAndExpectEqualScore(service, url,
                                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  NavigateWithTransitionAndExpectEqualScore(service, url,
                                            ui::PAGE_TRANSITION_LINK);
  NavigateWithTransitionAndExpectEqualScore(service, url,
                                            ui::PAGE_TRANSITION_RELOAD);
  NavigateWithTransitionAndExpectEqualScore(service, url,
                                            ui::PAGE_TRANSITION_FORM_SUBMIT);
}
