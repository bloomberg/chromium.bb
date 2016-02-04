// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath g_temp_history_dir;

const int kLessAccumulationsThanNeededToMaxDailyEngagement = 2;
const int kMoreAccumulationsThanNeededToMaxDailyEngagement = 40;
const int kMoreAccumulationsThanNeededToMaxTotalEngagement = 200;
const int kLessDaysThanNeededToMaxTotalEngagement = 4;
const int kMoreDaysThanNeededToMaxTotalEngagement = 40;
const int kLessPeriodsThanNeededToDecayMaxScore = 2;
const int kMorePeriodsThanNeededToDecayMaxScore = 40;

// Waits until a change is observed in site engagement content settings.
class SiteEngagementChangeWaiter : public content_settings::Observer {
 public:
  explicit SiteEngagementChangeWaiter(Profile* profile) : profile_(profile) {
    HostContentSettingsMapFactory::GetForProfile(profile)->AddObserver(this);
  }
  ~SiteEngagementChangeWaiter() override {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->RemoveObserver(this);
  }

  // Overridden from content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override {
    if (content_type == CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT)
      Proceed();
  }

  void Wait() { run_loop_.Run(); }

 private:
  void Proceed() { run_loop_.Quit(); }

  Profile* profile_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementChangeWaiter);
};

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

scoped_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  scoped_ptr<history::HistoryService> service(new history::HistoryService());
  service->Init(std::string(),
                history::TestHistoryDatabaseParamsForPath(g_temp_history_dir));
  return std::move(service);
}

}  // namespace

class SiteEngagementScoreTest : public testing::Test {
 public:
  SiteEngagementScoreTest() : score_(&test_clock_) {}

  void SetUp() override {
    testing::Test::SetUp();
    // Disable the first engagement bonus for tests.
    SiteEngagementScore::SetParamValuesForTesting();
  }

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

  void SetFirstDailyEngagementPointsForTesting(double points) {
    SiteEngagementScore::param_values
        [SiteEngagementScore::FIRST_DAILY_ENGAGEMENT] = points;
  }

  base::SimpleTestClock test_clock_;
  SiteEngagementScore score_;
};

// Accumulate score many times on the same day. Ensure each time the score goes
// up, but not more than the maximum per day.
TEST_F(SiteEngagementScoreTest, AccumulateOnSameDay) {
  base::Time reference_time = GetReferenceTime();

  test_clock_.SetNow(reference_time);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
    EXPECT_EQ(std::min(SiteEngagementScore::GetMaxPointsPerDay(),
                       (i + 1) * SiteEngagementScore::GetNavigationPoints()),
              score_.Score());
  }

  EXPECT_EQ(SiteEngagementScore::GetMaxPointsPerDay(), score_.Score());
}

// Accumulate on the first day to max that day's engagement, then accumulate on
// a different day.
TEST_F(SiteEngagementScoreTest, AccumulateOnTwoDays) {
  base::Time reference_time = GetReferenceTime();
  base::Time later_date = reference_time + base::TimeDelta::FromDays(2);

  test_clock_.SetNow(reference_time);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i)
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());

  EXPECT_EQ(SiteEngagementScore::GetMaxPointsPerDay(), score_.Score());

  test_clock_.SetNow(later_date);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
    double day_score =
        std::min(SiteEngagementScore::GetMaxPointsPerDay(),
                 (i + 1) * SiteEngagementScore::GetNavigationPoints());
    EXPECT_EQ(day_score + SiteEngagementScore::GetMaxPointsPerDay(),
              score_.Score());
  }

  EXPECT_EQ(2 * SiteEngagementScore::GetMaxPointsPerDay(), score_.Score());
}

// Accumulate score on many consecutive days and ensure the score doesn't exceed
// the maximum allowed.
TEST_F(SiteEngagementScoreTest, AccumulateALotOnManyDays) {
  base::Time current_day = GetReferenceTime();

  for (int i = 0; i < kMoreDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    test_clock_.SetNow(current_day);
    for (int j = 0; j < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::GetNavigationPoints());

    EXPECT_EQ(std::min(SiteEngagementScore::kMaxPoints,
                       (i + 1) * SiteEngagementScore::GetMaxPointsPerDay()),
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
      score_.AddPoints(SiteEngagementScore::GetNavigationPoints());

    EXPECT_EQ(
        std::min(SiteEngagementScore::kMaxPoints,
                 (i + 1) * kLessAccumulationsThanNeededToMaxDailyEngagement *
                     SiteEngagementScore::GetNavigationPoints()),
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
      score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());

  // The score should not have decayed before the first decay period has
  // elapsed.
  test_clock_.SetNow(current_day +
                     base::TimeDelta::FromDays(
                         SiteEngagementScore::GetDecayPeriodInDays() - 1));
  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.Score());

  // The score should have decayed by one chunk after one decay period has
  // elapsed.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(SiteEngagementScore::GetDecayPeriodInDays()));
  EXPECT_EQ(
      SiteEngagementScore::kMaxPoints - SiteEngagementScore::GetDecayPoints(),
      score_.Score());

  // The score should have decayed by the right number of chunks after a few
  // decay periods have elapsed.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(kLessPeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::GetDecayPeriodInDays()));
  EXPECT_EQ(SiteEngagementScore::kMaxPoints -
                kLessPeriodsThanNeededToDecayMaxScore *
                    SiteEngagementScore::GetDecayPoints(),
            score_.Score());

  // The score should not decay below zero.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(kMorePeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::GetDecayPeriodInDays()));
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
      score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
  }

  double initial_score = kLessDaysThanNeededToMaxTotalEngagement *
                         SiteEngagementScore::GetMaxPointsPerDay();
  EXPECT_EQ(initial_score, score_.Score());

  // Go forward a few decay periods.
  test_clock_.SetNow(
      current_day +
      base::TimeDelta::FromDays(kLessPeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::GetDecayPeriodInDays()));

  double decayed_score = initial_score -
                         kLessPeriodsThanNeededToDecayMaxScore *
                             SiteEngagementScore::GetDecayPoints();
  EXPECT_EQ(decayed_score, score_.Score());

  // Now add some points.
  score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
  EXPECT_EQ(decayed_score + SiteEngagementScore::GetNavigationPoints(),
            score_.Score());
}

// Test that going back in time is handled properly.
TEST_F(SiteEngagementScoreTest, GoBackInTime) {
  base::Time current_day = GetReferenceTime();

  test_clock_.SetNow(current_day);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i)
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());

  EXPECT_EQ(SiteEngagementScore::GetMaxPointsPerDay(), score_.Score());

  // Adding to the score on an earlier date should be treated like another day,
  // and should not cause any decay.
  test_clock_.SetNow(current_day - base::TimeDelta::FromDays(
                                       kMorePeriodsThanNeededToDecayMaxScore *
                                       SiteEngagementScore::GetDecayPoints()));
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
    double day_score =
        std::min(SiteEngagementScore::GetMaxPointsPerDay(),
                 (i + 1) * SiteEngagementScore::GetNavigationPoints());
    EXPECT_EQ(day_score + SiteEngagementScore::GetMaxPointsPerDay(),
              score_.Score());
  }

  EXPECT_EQ(2 * SiteEngagementScore::GetMaxPointsPerDay(), score_.Score());
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

// Ensure bonus engagement is awarded for the first engagement of a day.
TEST_F(SiteEngagementScoreTest, FirstDailyEngagementBonus) {
  SetFirstDailyEngagementPointsForTesting(0.5);

  SiteEngagementScore score1(&test_clock_);
  SiteEngagementScore score2(&test_clock_);
  base::Time current_day = GetReferenceTime();

  test_clock_.SetNow(current_day);

  // The first engagement event gets the bonus.
  score1.AddPoints(0.5);
  EXPECT_EQ(1.0, score1.Score());

  // Subsequent events do not.
  score1.AddPoints(0.5);
  EXPECT_EQ(1.5, score1.Score());

  // Bonuses are awarded independently between scores.
  score2.AddPoints(1.0);
  EXPECT_EQ(1.5, score2.Score());
  score2.AddPoints(1.0);
  EXPECT_EQ(2.5, score2.Score());

  test_clock_.SetNow(current_day + base::TimeDelta::FromDays(1));

  // The first event for the next day gets the bonus.
  score1.AddPoints(0.5);
  EXPECT_EQ(2.5, score1.Score());

  // Subsequent events do not.
  score1.AddPoints(0.5);
  EXPECT_EQ(3.0, score1.Score());

  score2.AddPoints(1.0);
  EXPECT_EQ(4.0, score2.Score());
  score2.AddPoints(1.0);
  EXPECT_EQ(5.0, score2.Score());
}

// Test that resetting a score has the correct properties.
TEST_F(SiteEngagementScoreTest, Reset) {
  base::Time current_day = GetReferenceTime();

  test_clock_.SetNow(current_day);
  score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
  EXPECT_EQ(SiteEngagementScore::GetNavigationPoints(), score_.Score());

  current_day += base::TimeDelta::FromDays(7);
  test_clock_.SetNow(current_day);

  score_.Reset(20.0);
  EXPECT_DOUBLE_EQ(20.0, score_.Score());
  EXPECT_DOUBLE_EQ(0, score_.points_added_today_);
  EXPECT_EQ(current_day, score_.last_engagement_time_);

  // Adding points after the reset should work as normal.
  score_.AddPoints(5);
  EXPECT_EQ(25.0, score_.Score());

  // The decay should happen one decay period from
  test_clock_.SetNow(current_day +
                     base::TimeDelta::FromDays(
                         SiteEngagementScore::GetDecayPeriodInDays() + 1));
  EXPECT_EQ(25.0 - SiteEngagementScore::GetDecayPoints(), score_.Score());
}

class SiteEngagementServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    g_temp_history_dir = temp_dir_.path();
    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile(), &BuildTestHistoryService);
    SiteEngagementScore::SetParamValuesForTesting();
  }

  void NavigateWithTransitionAndExpectHigherScore(
      SiteEngagementService* service,
      const GURL& url,
      ui::PageTransition transition) {
    double prev_score = service->GetScore(url);
    controller().LoadURL(url, content::Referrer(), transition, std::string());
    int pending_id = controller().GetPendingEntry()->GetUniqueID();
    content::WebContentsTester::For(web_contents())
        ->TestDidNavigate(web_contents()->GetMainFrame(), 1, pending_id, true,
                          url, transition);
    EXPECT_LT(prev_score, service->GetScore(url));
  }

  void NavigateWithTransitionAndExpectEqualScore(
      SiteEngagementService* service,
      const GURL& url,
      ui::PageTransition transition) {
    double prev_score = service->GetScore(url);
    controller().LoadURL(url, content::Referrer(), transition, std::string());
    int pending_id = controller().GetPendingEntry()->GetUniqueID();
    content::WebContentsTester::For(web_contents())
        ->TestDidNavigate(web_contents()->GetMainFrame(), 1, pending_id, true,
                          url, transition);
    EXPECT_EQ(prev_score, service->GetScore(url));
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(SiteEngagementServiceTest, GetMedianEngagement) {
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

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
  ASSERT_TRUE(service);

  // Create the helper manually since it isn't present when a tab isn't created.
  SiteEngagementHelper::CreateForWebContents(web_contents());

  GURL url("http://www.google.com/");
  EXPECT_EQ(0, service->GetScore(url));
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
  ASSERT_TRUE(service);

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
  ASSERT_TRUE(service);

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

TEST_F(SiteEngagementServiceTest, LastShortcutLaunch) {
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  scoped_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), make_scoped_ptr(clock)));

  base::HistogramTester histograms;

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day - base::TimeDelta::FromDays(5));

  // The https and http versions of www.google.com should be separate. But
  // different paths on the same origin should be treated the same.
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  GURL url3("http://www.google.com/maps");

  EXPECT_EQ(0, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_EQ(0, service->GetScore(url3));

  service->SetLastShortcutLaunchTime(url2);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kDaysSinceLastShortcutLaunchHistogram, 0);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH, 1);

  service->AddPoints(url1, 2.0);
  service->AddPoints(url2, 2.0);
  clock->SetNow(current_day);
  service->SetLastShortcutLaunchTime(url2);

  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kDaysSinceLastShortcutLaunchHistogram, 1);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementTypeHistogram, 4);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH, 2);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 2);

  EXPECT_DOUBLE_EQ(2.0, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(7.0, service->GetScore(url2));

  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromDays(1));
  EXPECT_DOUBLE_EQ(2.0, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(7.0, service->GetScore(url2));

  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromDays(7));
  EXPECT_DOUBLE_EQ(0.0, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(5.0, service->GetScore(url2));

  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromDays(10));
  EXPECT_DOUBLE_EQ(0.0, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(5.0, service->GetScore(url2));

  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromDays(11));
  EXPECT_DOUBLE_EQ(0.0, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.0, service->GetScore(url2));
}

TEST_F(SiteEngagementServiceTest, CheckHistograms) {
  base::HistogramTester histograms;

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  scoped_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), make_scoped_ptr(clock)));

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

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

  // Record metrics for an empty engagement system.
  service->RecordMetrics();

  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kTotalEngagementHistogram, 0, 1);
  histograms.ExpectUniqueSample(SiteEngagementMetrics::kTotalOriginsHistogram,
                                0, 1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementScoreHistogram,
                              0);
  histograms.ExpectUniqueSample(SiteEngagementMetrics::kMeanEngagementHistogram,
                                0, 1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kMedianEngagementHistogram, 0, 1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram, 0, 1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram, 0, 1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              0);

  const std::vector<std::string> engagement_bucket_histogram_names =
      SiteEngagementMetrics::GetEngagementBucketHistogramNames();

  for (const std::string& histogram_name : engagement_bucket_histogram_names)
    histograms.ExpectTotalCount(histogram_name, 0);

  clock->SetNow(clock->Now() + base::TimeDelta::FromMinutes(60));

  // The https and http versions of www.google.com should be separate.
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  GURL url3("http://drive.google.com/");

  service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);
  service->HandleUserInput(url1, SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  service->HandleUserInput(url1, SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  service->HandleMediaPlaying(url2, true);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kTotalEngagementHistogram,
                              2);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kTotalOriginsHistogram, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 0,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 1,
                               1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMeanEngagementHistogram,
                              2);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMedianEngagementHistogram,
                              2);
  // Recorded per origin.
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementScoreHistogram,
                              1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram, 0, 2);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram, 0, 2);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram, 0, 2);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              6);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_HIDDEN,
                               1);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 2);

  // Navigations are still logged within the 1 hour refresh period
  clock->SetNow(clock->Now() + base::TimeDelta::FromMinutes(59));

  service->HandleNavigation(url2, ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(url2, ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              8);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_HIDDEN,
                               1);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 2);

  // Update the hourly histograms again.
  clock->SetNow(clock->Now() + base::TimeDelta::FromMinutes(1));

  service->HandleNavigation(url3, ui::PAGE_TRANSITION_TYPED);
  service->HandleUserInput(url2,
                           SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE);
  service->HandleMediaPlaying(url3, false);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kTotalEngagementHistogram,
                              3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 0,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 1,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 3,
                               1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMeanEngagementHistogram,
                              3);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMedianEngagementHistogram,
                              3);
  // Recorded per origin.
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementScoreHistogram,
                              4);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram, 0, 3);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram, 0, 3);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram, 0, 3);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              12);
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
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 3);

  service->HandleNavigation(url1, ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);
  service->HandleUserInput(url2, SiteEngagementMetrics::ENGAGEMENT_WHEEL);
  service->HandleUserInput(url1, SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  service->HandleUserInput(url3, SiteEngagementMetrics::ENGAGEMENT_MOUSE);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              17);
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
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 3);

  // Advance an origin to the max for a day and advance the clock an hour before
  // the last increment before max. Expect the histogram to be updated.
  for (int i = 0; i < 6; ++i)
    service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);

  clock->SetNow(clock->Now() + base::TimeDelta::FromMinutes(60));
  service->HandleNavigation(url1, ui::PAGE_TRANSITION_TYPED);

  histograms.ExpectTotalCount(SiteEngagementMetrics::kTotalEngagementHistogram,
                              4);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 0,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 1,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kTotalOriginsHistogram, 3,
                               2);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMeanEngagementHistogram,
                              4);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kMedianEngagementHistogram,
                              4);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementScoreHistogram,
                              7);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram, 0, 4);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram, 0, 3);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram, 1, 1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram, 0, 4);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              24);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION,
                               13);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 3);

  for (const std::string& histogram_name : engagement_bucket_histogram_names)
    histograms.ExpectTotalCount(histogram_name, 3);

  histograms.ExpectBucketCount(engagement_bucket_histogram_names[0], 100, 1);
  histograms.ExpectBucketCount(engagement_bucket_histogram_names[0], 33, 1);
  histograms.ExpectBucketCount(engagement_bucket_histogram_names[0], 66, 1);
  histograms.ExpectBucketCount(engagement_bucket_histogram_names[1], 33, 1);
  histograms.ExpectBucketCount(engagement_bucket_histogram_names[1], 66, 1);
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
        base::TimeDelta::FromDays(SiteEngagementScore::GetDecayPeriodInDays()));

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
                      3 * SiteEngagementScore::GetDecayPeriodInDays()));

    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_EQ(1u, score_map.size());
    EXPECT_EQ(0, score_map[url1]);

    service->CleanupEngagementScores();

    score_map = service->GetScoreMap();
    EXPECT_EQ(0u, score_map.size());
    EXPECT_EQ(0, service->GetScore(url1));
  }
}

TEST_F(SiteEngagementServiceTest, NavigationAccumulation) {
  GURL url("https://www.google.com/");

  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  // Create the helper manually since it isn't present when a tab isn't created.
  SiteEngagementHelper::CreateForWebContents(web_contents());

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

TEST_F(SiteEngagementServiceTest, IsBootstrapped) {
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  scoped_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), make_scoped_ptr(clock)));

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

  GURL url1("https://www.google.com/");
  GURL url2("https://www.somewhereelse.com/");

  EXPECT_FALSE(service->IsBootstrapped());

  service->AddPoints(url1, 5.0);
  EXPECT_FALSE(service->IsBootstrapped());

  service->AddPoints(url2, 5.0);
  EXPECT_TRUE(service->IsBootstrapped());

  clock->SetNow(current_day + base::TimeDelta::FromDays(10));
  EXPECT_FALSE(service->IsBootstrapped());
}

TEST_F(SiteEngagementServiceTest, CleanupOriginsOnHistoryDeletion) {
  SiteEngagementService* engagement =
      SiteEngagementServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(engagement);

  GURL origin1("http://www.google.com/");
  GURL origin1a("http://www.google.com/search?q=asdf");
  GURL origin2("https://drive.google.com/");
  GURL origin2a("https://drive.google.com/somedoc");
  GURL origin3("http://notdeleted.com/");

  base::Time today = GetReferenceTime();
  base::Time yesterday = GetReferenceTime() - base::TimeDelta::FromDays(1);
  base::Time yesterday_afternoon = GetReferenceTime() -
                                   base::TimeDelta::FromDays(1) +
                                   base::TimeDelta::FromHours(4);

  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);

  history->AddPage(origin1, yesterday_afternoon, history::SOURCE_BROWSED);
  history->AddPage(origin1a, today, history::SOURCE_BROWSED);
  engagement->AddPoints(origin1, 5.0);

  history->AddPage(origin2, yesterday_afternoon, history::SOURCE_BROWSED);
  history->AddPage(origin2a, yesterday_afternoon, history::SOURCE_BROWSED);
  engagement->AddPoints(origin2, 5.0);

  history->AddPage(origin3, today, history::SOURCE_BROWSED);
  engagement->AddPoints(origin3, 5.0);

  EXPECT_EQ(5.0, engagement->GetScore(origin1));
  EXPECT_EQ(5.0, engagement->GetScore(origin2));
  EXPECT_EQ(5.0, engagement->GetScore(origin3));

  {
    SiteEngagementChangeWaiter waiter(profile());

    base::CancelableTaskTracker task_tracker;
    // Expire origin1, origin2 and origin2a.
    history->ExpireHistoryBetween(std::set<GURL>(), yesterday, today,
                                  base::Bind(&base::DoNothing), &task_tracker);
    waiter.Wait();

    // origin2 is cleaned up because all its urls are deleted. origin1a is still
    // in history, maintaining origin1's score. origin3 is untouched.
    EXPECT_EQ(5.0, engagement->GetScore(origin1));
    EXPECT_EQ(0, engagement->GetScore(origin2));
    EXPECT_EQ(5.0, engagement->GetScore(origin3));
    EXPECT_EQ(10.0, engagement->GetTotalEngagementPoints());
  }

  {
    SiteEngagementChangeWaiter waiter(profile());

    // Expire origin1a.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(origin1a);
    args.SetTimeRangeForOneDay(today);
    expire_list.push_back(args);

    base::CancelableTaskTracker task_tracker;
    history->ExpireHistory(expire_list, base::Bind(&base::DoNothing),
                           &task_tracker);
    waiter.Wait();

    // Only origin3 remains.
    EXPECT_EQ(0, engagement->GetScore(origin1));
    EXPECT_EQ(0, engagement->GetScore(origin2));
    EXPECT_EQ(5.0, engagement->GetScore(origin3));
    EXPECT_EQ(5.0, engagement->GetTotalEngagementPoints());
  }
}
