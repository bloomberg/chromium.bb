// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "chrome/browser/engagement/site_engagement_score.h"
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

const int kMoreAccumulationsThanNeededToMaxDailyEngagement = 40;
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
    HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(
        this);
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

std::unique_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  std::unique_ptr<history::HistoryService> service(
      new history::HistoryService());
  service->Init(history::TestHistoryDatabaseParamsForPath(g_temp_history_dir));
  return std::move(service);
}

}  // namespace

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
  SiteEngagementService* service = SiteEngagementService::Get(profile());
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
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  // Create the helper manually since it isn't present when a tab isn't created.
  SiteEngagementService::Helper::CreateForWebContents(web_contents());

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
  SiteEngagementService* service = SiteEngagementService::Get(profile());
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
  SiteEngagementService* service = SiteEngagementService::Get(profile());
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

  service->HandleUserInput(url2, SiteEngagementMetrics::ENGAGEMENT_SCROLL);
  service->HandleUserInput(url3,
                           SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE);
  EXPECT_DOUBLE_EQ(0.15, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(0.1, service->GetScore(url3));
  EXPECT_DOUBLE_EQ(0.4, service->GetTotalEngagementPoints());
}

TEST_F(SiteEngagementServiceTest, LastShortcutLaunch) {
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

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
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              4);
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
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

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
      SiteEngagementMetrics::kEngagementScoreHistogramHTTP, 0);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTPS, 0);
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
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTP, 0);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTPS, 0);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementPercentageForHTTPSHistogram, 0);
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
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTP, 0);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTPS, 1);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementPercentageForHTTPSHistogram, 1);
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
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTP, 2);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTPS, 2);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementPercentageForHTTPSHistogram, 2);
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
  service->HandleUserInput(url2, SiteEngagementMetrics::ENGAGEMENT_SCROLL);
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
                               SiteEngagementMetrics::ENGAGEMENT_SCROLL, 1);
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
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTP, 4);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTPS, 3);
  histograms.ExpectTotalCount(
      SiteEngagementMetrics::kEngagementScoreHistogramHTTPS, 3);
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
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

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

  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  // Create the helper manually since it isn't present when a tab isn't created.
  SiteEngagementService::Helper::CreateForWebContents(web_contents());

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
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

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
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> engagement(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));
  ASSERT_TRUE(engagement.get());

  GURL origin1("http://www.google.com/");
  GURL origin1a("http://www.google.com/search?q=asdf");
  GURL origin1b("http://www.google.com/maps/search?q=asdf");
  GURL origin2("https://drive.google.com/");
  GURL origin2a("https://drive.google.com/somedoc");
  GURL origin3("http://notdeleted.com/");
  GURL origin4("http://decayed.com/");
  GURL origin4a("http://decayed.com/index.html");

  base::Time today = GetReferenceTime();
  base::Time yesterday = GetReferenceTime() - base::TimeDelta::FromDays(1);
  base::Time yesterday_afternoon = GetReferenceTime() -
                                   base::TimeDelta::FromDays(1) +
                                   base::TimeDelta::FromHours(4);
  base::Time yesterday_week = GetReferenceTime() - base::TimeDelta::FromDays(8);
  clock->SetNow(today);

  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);

  history->AddPage(origin1, yesterday_afternoon, history::SOURCE_BROWSED);
  history->AddPage(origin1a, yesterday_week, history::SOURCE_BROWSED);
  history->AddPage(origin1b, today, history::SOURCE_BROWSED);
  engagement->AddPoints(origin1, 3.0);

  history->AddPage(origin2, yesterday_afternoon, history::SOURCE_BROWSED);
  history->AddPage(origin2a, yesterday_afternoon, history::SOURCE_BROWSED);
  engagement->AddPoints(origin2, 5.0);

  history->AddPage(origin3, today, history::SOURCE_BROWSED);
  engagement->AddPoints(origin3, 5.0);

  history->AddPage(origin4, yesterday_week, history::SOURCE_BROWSED);
  history->AddPage(origin4a, yesterday_afternoon, history::SOURCE_BROWSED);
  engagement->AddPoints(origin4, 5.0);

  EXPECT_EQ(3.0, engagement->GetScore(origin1));
  EXPECT_EQ(5.0, engagement->GetScore(origin2));
  EXPECT_EQ(5.0, engagement->GetScore(origin3));
  EXPECT_EQ(5.0, engagement->GetScore(origin4));

  {
    SiteEngagementChangeWaiter waiter(profile());

    base::CancelableTaskTracker task_tracker;
    // Expire origin1, origin2, origin2a, and origin4's most recent visit.
    history->ExpireHistoryBetween(std::set<GURL>(), yesterday, today,
                                  base::Bind(&base::DoNothing), &task_tracker);
    waiter.Wait();

    // origin2 is cleaned up because all its urls are deleted. origin1a and
    // origin1b are still in history, but 33% of urls have been deleted, thus
    // cutting origin1's score by 1/3. origin3 is untouched. origin4 has 1 URL
    // deleted and 1 remaining, but its most recent visit is more than 1 week in
    // the past. Ensure that its scored is halved, and not decayed further.
    EXPECT_EQ(2, engagement->GetScore(origin1));
    EXPECT_EQ(0, engagement->GetScore(origin2));
    EXPECT_EQ(5.0, engagement->GetScore(origin3));
    EXPECT_EQ(2.5, engagement->GetScore(origin4));
    EXPECT_EQ(9.5, engagement->GetTotalEngagementPoints());
  }

  {
    SiteEngagementChangeWaiter waiter(profile());

    // Expire origin1a.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(origin1a);
    args.SetTimeRangeForOneDay(yesterday_week);
    expire_list.push_back(args);

    base::CancelableTaskTracker task_tracker;
    history->ExpireHistory(expire_list, base::Bind(&base::DoNothing),
                           &task_tracker);
    waiter.Wait();

    // origin1's score should be halved again. origin3 and origin4 remain
    // untouched.
    EXPECT_EQ(1, engagement->GetScore(origin1));
    EXPECT_EQ(0, engagement->GetScore(origin2));
    EXPECT_EQ(5.0, engagement->GetScore(origin3));
    EXPECT_EQ(2.5, engagement->GetScore(origin4));
    EXPECT_EQ(8.5, engagement->GetTotalEngagementPoints());
  }

  {
    SiteEngagementChangeWaiter waiter(profile());

    // Expire origin1b.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(origin1b);
    args.SetTimeRangeForOneDay(today);
    expire_list.push_back(args);

    base::CancelableTaskTracker task_tracker;
    history->ExpireHistory(expire_list, base::Bind(&base::DoNothing),
                           &task_tracker);
    waiter.Wait();

    // origin1 should be removed. origin3 and origin4 remain untouched.
    EXPECT_EQ(0, engagement->GetScore(origin1));
    EXPECT_EQ(0, engagement->GetScore(origin2));
    EXPECT_EQ(5.0, engagement->GetScore(origin3));
    EXPECT_EQ(2.5, engagement->GetScore(origin4));
    EXPECT_EQ(7.5, engagement->GetTotalEngagementPoints());
  }
}

TEST_F(SiteEngagementServiceTest, EngagementLevel) {
  static_assert(SiteEngagementService::ENGAGEMENT_LEVEL_NONE !=
                    SiteEngagementService::ENGAGEMENT_LEVEL_LOW,
                "enum values should not be equal");
  static_assert(SiteEngagementService::ENGAGEMENT_LEVEL_LOW !=
                    SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM,
                "enum values should not be equal");
  static_assert(SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM !=
                    SiteEngagementService::ENGAGEMENT_LEVEL_HIGH,
                "enum values should not be equal");
  static_assert(SiteEngagementService::ENGAGEMENT_LEVEL_HIGH !=
                    SiteEngagementService::ENGAGEMENT_LEVEL_MAX,
                "enum values should not be equal");

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");

  EXPECT_EQ(SiteEngagementService::ENGAGEMENT_LEVEL_NONE,
            service->GetEngagementLevel(url1));
  EXPECT_EQ(SiteEngagementService::ENGAGEMENT_LEVEL_NONE,
            service->GetEngagementLevel(url2));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_NONE));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_LOW));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_HIGH));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_MAX));

  // Bring url1 to LOW engagement.
  service->AddPoints(url1, 1.0);
  EXPECT_EQ(SiteEngagementService::ENGAGEMENT_LEVEL_LOW,
            service->GetEngagementLevel(url1));
  EXPECT_EQ(SiteEngagementService::ENGAGEMENT_LEVEL_NONE,
            service->GetEngagementLevel(url2));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_NONE));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_LOW));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_HIGH));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, SiteEngagementService::ENGAGEMENT_LEVEL_MAX));

  // Bring url2 to MEDIUM engagement.
  service->AddPoints(url2, 5.0);
  EXPECT_EQ(SiteEngagementService::ENGAGEMENT_LEVEL_LOW,
            service->GetEngagementLevel(url1));
  EXPECT_EQ(SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM,
            service->GetEngagementLevel(url2));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_NONE));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_LOW));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_HIGH));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_MAX));

  // Bring url2 to HIGH engagement.
  for (int i = 0; i < 9; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    clock->SetNow(current_day);
    service->AddPoints(url2, 5.0);
  }
  EXPECT_EQ(SiteEngagementService::ENGAGEMENT_LEVEL_HIGH,
            service->GetEngagementLevel(url2));

  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_NONE));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_LOW));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_HIGH));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_MAX));

  // Bring url2 to MAX engagement.
  for (int i = 0; i < 10; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    clock->SetNow(current_day);
    service->AddPoints(url2, 5.0);
  }
  EXPECT_EQ(SiteEngagementService::ENGAGEMENT_LEVEL_MAX,
            service->GetEngagementLevel(url2));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_NONE));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_LOW));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_HIGH));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, SiteEngagementService::ENGAGEMENT_LEVEL_MAX));
}

TEST_F(SiteEngagementServiceTest, ScoreDecayHistograms) {
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);
  base::HistogramTester histograms;
  GURL origin1("http://www.google.com/");
  GURL origin2("http://drive.google.com/");

  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              0);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              0);

  service->AddPoints(origin2, SiteEngagementScore::GetNavigationPoints());

  // Max the score for origin1.
  for (int i = 0; i < kMoreDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    clock->SetNow(current_day);

    for (int j = 0; j < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++j)
      service->AddPoints(origin1, SiteEngagementScore::GetNavigationPoints());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, service->GetScore(origin1));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              0);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              0);

  // Check histograms after one decay period.
  clock->SetNow(current_day + base::TimeDelta::FromDays(
                                  SiteEngagementScore::GetDecayPeriodInDays()));

  // Trigger decay and histogram hit.
  service->AddPoints(origin1, 0.01);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kScoreDecayedFromHistogram,
      SiteEngagementScore::kMaxPoints, 1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kScoreDecayedToHistogram,
      SiteEngagementScore::kMaxPoints - SiteEngagementScore::GetDecayPoints(),
      1);

  // Check histograms after a few decay periods.
  clock->SetNow(current_day + base::TimeDelta::FromDays(
                                  kLessPeriodsThanNeededToDecayMaxScore *
                                  SiteEngagementScore::GetDecayPeriodInDays()));
  // Trigger decay and histogram hit.
  service->AddPoints(origin1, 0.01);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              2);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              2);

  // Check decay to zero.
  clock->SetNow(current_day + base::TimeDelta::FromDays(
                                  kMorePeriodsThanNeededToDecayMaxScore *
                                  SiteEngagementScore::GetDecayPeriodInDays()));
  // Trigger decay and histogram hit.
  service->AddPoints(origin1, 0.01);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              3);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                               0, 1);
  // Trigger decay and histogram hit for origin2, checking an independent decay.
  service->AddPoints(origin2, 0.01);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              4);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              4);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kScoreDecayedFromHistogram, 0, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                               0, 2);

  // Add more points and ensure no more samples are present.
  service->AddPoints(origin1, 0.01);
  service->AddPoints(origin2, 0.01);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              4);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              4);
}
