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
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath g_temp_history_dir;

const int kMoreAccumulationsThanNeededToMaxDailyEngagement = 40;
const int kMoreDaysThanNeededToMaxTotalEngagement = 40;
const int kMorePeriodsThanNeededToDecayMaxScore = 40;
const double kMaxRoundingDeviation = 0.0001;

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

  base::Time out_time;
  EXPECT_TRUE(
      base::Time::FromLocalExploded(exploded_reference_time, &out_time));
  return out_time;
}

std::unique_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  std::unique_ptr<history::HistoryService> service(
      new history::HistoryService());
  service->Init(history::TestHistoryDatabaseParamsForPath(g_temp_history_dir));
  return std::move(service);
}

}  // namespace

class ObserverTester : public SiteEngagementObserver {
 public:
  ObserverTester(SiteEngagementService* service,
                 content::WebContents* web_contents,
                 const GURL& url,
                 double score)
      : SiteEngagementObserver(service),
        web_contents_(web_contents),
        url_(url),
        score_(score),
        callback_called_(false),
        run_loop_() {}

  void OnEngagementIncreased(content::WebContents* web_contents,
                             const GURL& url,
                             double score) override {
    EXPECT_EQ(web_contents_, web_contents);
    EXPECT_EQ(url_, url);
    EXPECT_DOUBLE_EQ(score_, score);
    set_callback_called(true);
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  bool callback_called() { return callback_called_; }
  void set_callback_called(bool callback_called) {
    callback_called_ = callback_called;
  }

 private:
  content::WebContents* web_contents_;
  GURL url_;
  double score_;
  bool callback_called_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ObserverTester);
};

class SiteEngagementServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    g_temp_history_dir = temp_dir_.GetPath();
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
        ->TestDidNavigate(web_contents()->GetMainFrame(), pending_id, true,
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
        ->TestDidNavigate(web_contents()->GetMainFrame(), pending_id, true,
                          url, transition);
    EXPECT_EQ(prev_score, service->GetScore(url));
  }

  void SetParamValue(SiteEngagementScore::Variation variation, double value) {
    SiteEngagementScore::GetParamValues()[variation].second = value;
  }

  void AssertInRange(double expected, double actual) {
    EXPECT_NEAR(expected, actual, kMaxRoundingDeviation);
  }

  double CheckScoreFromSettingsOnThread(
      content::BrowserThread::ID thread_id,
      HostContentSettingsMap* settings_map,
      const GURL& url) {
    double score = 0;
    base::RunLoop run_loop;
    content::BrowserThread::PostTaskAndReply(
        thread_id, FROM_HERE,
        base::Bind(&SiteEngagementServiceTest::CheckScoreFromSettings,
                   base::Unretained(this), settings_map, url, &score),
                   run_loop.QuitClosure());
    run_loop.Run();
    return score;
  }

 private:
  void CheckScoreFromSettings(HostContentSettingsMap* settings_map,
                              const GURL& url,
                              double *score) {
    *score = SiteEngagementService::GetScoreFromSettings(settings_map, url);
  }

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

  NavigateAndCommit(url1);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0.5, service->GetTotalEngagementPoints());

  NavigateAndCommit(url2);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(web_contents(),
                            ui::PAGE_TRANSITION_KEYWORD_GENERATED);
  EXPECT_EQ(1, service->GetScore(url2));
  EXPECT_EQ(1.5, service->GetTotalEngagementPoints());

  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  EXPECT_EQ(1.5, service->GetScore(url2));
  EXPECT_EQ(2, service->GetTotalEngagementPoints());

  NavigateAndCommit(url3);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(0.5, service->GetScore(url3));
  EXPECT_EQ(2.5, service->GetTotalEngagementPoints());

  NavigateAndCommit(url1);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);
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

  NavigateAndCommit(url1);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  EXPECT_DOUBLE_EQ(0.05, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.05, service->GetTotalEngagementPoints());

  NavigateAndCommit(url2);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  EXPECT_DOUBLE_EQ(0.1, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(0.15, service->GetTotalEngagementPoints());

  NavigateAndCommit(url3);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  EXPECT_DOUBLE_EQ(0.05, service->GetScore(url3));
  EXPECT_DOUBLE_EQ(0.2, service->GetTotalEngagementPoints());

  NavigateAndCommit(url1);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  EXPECT_DOUBLE_EQ(0.15, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.3, service->GetTotalEngagementPoints());

  NavigateAndCommit(url2);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_SCROLL);
  NavigateAndCommit(url3);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE);
  EXPECT_DOUBLE_EQ(0.15, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(0.1, service->GetScore(url3));
  EXPECT_DOUBLE_EQ(0.4, service->GetTotalEngagementPoints());
}

TEST_F(SiteEngagementServiceTest, RestrictedToHTTPAndHTTPS) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  // The https and http versions of www.google.com should be separate.
  GURL url1("ftp://www.google.com/");
  GURL url2("file://blah");
  GURL url3("chrome://");
  GURL url4("about://config");

  NavigateAndCommit(url1);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  EXPECT_EQ(0, service->GetScore(url1));

  NavigateAndCommit(url2);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(0, service->GetScore(url2));

  NavigateAndCommit(url3);
  service->HandleMediaPlaying(web_contents(), true);
  EXPECT_EQ(0, service->GetScore(url3));

  NavigateAndCommit(url4);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  EXPECT_EQ(0, service->GetScore(url4));
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

  service->AddPoints(url1, 1.0);
  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromDays(10));
  EXPECT_DOUBLE_EQ(1.0, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(5.0, service->GetScore(url2));

  clock->SetNow(GetReferenceTime() + base::TimeDelta::FromDays(11));
  EXPECT_DOUBLE_EQ(1.0, service->GetScore(url1));
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

  NavigateAndCommit(url1);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  NavigateAndCommit(url2);
  service->HandleMediaPlaying(web_contents(), true);

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

  NavigateAndCommit(url2);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_AUTO_BOOKMARK);

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

  NavigateAndCommit(url3);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);
  service->HandleMediaPlaying(web_contents(), false);
  NavigateAndCommit(url2);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE);

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

  NavigateAndCommit(url1);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_GENERATED);
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);
  NavigateAndCommit(url2);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_SCROLL);
  NavigateAndCommit(url1);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  NavigateAndCommit(url3);
  service->HandleUserInput(web_contents(),
                           SiteEngagementMetrics::ENGAGEMENT_MOUSE);

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
  NavigateAndCommit(url1);
  for (int i = 0; i < 6; ++i)
    service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);

  clock->SetNow(clock->Now() + base::TimeDelta::FromMinutes(60));
  service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);

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

// Expect that sites that have reached zero engagement are cleaned up. Expect
// engagement times to be reset if too much time has passed since the last
// engagement.
TEST_F(SiteEngagementServiceTest, CleanupEngagementScores) {
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

  // Set the base time to be 3 weeks past the stale period in the past.
  // Use a 1 second offset to make sure scores don't yet decay.
  base::TimeDelta one_second = base::TimeDelta::FromSeconds(1);
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);
  base::TimeDelta decay_period =
      base::TimeDelta::FromHours(SiteEngagementScore::GetDecayPeriodInHours());
  base::TimeDelta shorter_than_decay_period = decay_period - one_second;

  base::Time max_decay_time = GetReferenceTime() - service->GetMaxDecayPeriod();
  base::Time stale_time = GetReferenceTime() - service->GetStalePeriod();
  base::Time base_time = stale_time - shorter_than_decay_period * 4;
  clock->SetNow(base_time);

  // The https and http versions of www.google.com should be separate.
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  GURL url3("http://maps.google.com/");
  GURL url4("http://drive.google.com/");

  EXPECT_EQ(0, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_EQ(0, service->GetScore(url3));
  EXPECT_EQ(0, service->GetScore(url4));

  // Add some points
  service->AddPoints(url1, 1.0);
  service->AddPoints(url2, 5.0);
  EXPECT_EQ(1.0, service->GetScore(url1));
  EXPECT_EQ(5.0, service->GetScore(url2));

  // Add more to url2 over the next few days. Leave it completely alone after
  // this.
  clock->SetNow(base_time + one_day);
  service->AddPoints(url2, 5.0);
  EXPECT_EQ(10.0, service->GetScore(url2));

  clock->SetNow(base_time + 2 * one_day);
  service->AddPoints(url2, 5.0);
  EXPECT_EQ(15.0, service->GetScore(url2));

  clock->SetNow(base_time + 3 * one_day);
  service->AddPoints(url2, 2.0);
  EXPECT_EQ(17.0, service->GetScore(url2));
  base::Time url2_last_modified = clock->Now();

  // Move to (3 * shorter_than_decay_period) before the stale period.
  base_time += shorter_than_decay_period;
  clock->SetNow(base_time);
  service->AddPoints(url1, 1.0);
  service->AddPoints(url3, 5.0);
  EXPECT_EQ(2.0, service->GetScore(url1));
  EXPECT_EQ(5.0, service->GetScore(url3));

  // Add more to url3, and then leave it alone.
  clock->SetNow(base_time + one_day);
  service->AddPoints(url1, 5.0);
  service->AddPoints(url3, 5.0);
  EXPECT_EQ(7.0, service->GetScore(url1));
  EXPECT_EQ(10.0, service->GetScore(url3));

  // Move to (2 * shorter_than_decay_period) before the stale period.
  base_time += shorter_than_decay_period;
  clock->SetNow(base_time);
  service->AddPoints(url1, 5.0);
  service->AddPoints(url4, 5.0);
  EXPECT_EQ(12.0, service->GetScore(url1));
  EXPECT_EQ(5.0, service->GetScore(url4));

  // Move to shorter_than_decay_period before the stale period.
  base_time += shorter_than_decay_period;
  clock->SetNow(base_time);
  service->AddPoints(url1, 1.5);
  service->AddPoints(url4, 2.0);
  EXPECT_EQ(13.5, service->GetScore(url1));
  EXPECT_EQ(7.0, service->GetScore(url4));

  // After cleanup, url2 should be last modified offset to max_decay_time by the
  // current offset to now.
  url2_last_modified = max_decay_time - (clock->Now() - url2_last_modified);
  base_time = GetReferenceTime();

  {
    clock->SetNow(base_time);
    ASSERT_TRUE(service->IsLastEngagementStale());

    // Run a cleanup. Last engagement times will be reset relative to
    // max_decay_time. After the reset, url2 will go through 3 decays, url3
    // will go through 2 decays, and url1/url4 will go through 1 decay. This
    // decay is uncommitted!
    service->CleanupEngagementScores(true);
    ASSERT_FALSE(service->IsLastEngagementStale());

    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_EQ(3u, score_map.size());
    EXPECT_EQ(8.5, score_map[url1]);
    EXPECT_EQ(2.0, score_map[url2]);
    EXPECT_EQ(2.0, score_map[url4]);
    EXPECT_EQ(0, service->GetScore(url3));

    EXPECT_EQ(max_decay_time,
              service->CreateEngagementScore(url1).last_engagement_time());
    EXPECT_EQ(url2_last_modified,
              service->CreateEngagementScore(url2).last_engagement_time());
    EXPECT_EQ(max_decay_time,
              service->CreateEngagementScore(url4).last_engagement_time());
    EXPECT_EQ(max_decay_time, service->GetLastEngagementTime());
  }

  {
    // Advance time by the stale period. Nothing should happen in the cleanup.
    // Last engagement times are now relative to max_decay_time + stale period
    base_time += service->GetStalePeriod();
    clock->SetNow(base_time);
    ASSERT_TRUE(service->IsLastEngagementStale());

    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_EQ(3u, score_map.size());
    EXPECT_EQ(8.5, score_map[url1]);
    EXPECT_EQ(2.0, score_map[url2]);
    EXPECT_EQ(2.0, score_map[url4]);

    EXPECT_EQ(max_decay_time + service->GetStalePeriod(),
              service->CreateEngagementScore(url1).last_engagement_time());
    EXPECT_EQ(url2_last_modified + service->GetStalePeriod(),
              service->CreateEngagementScore(url2).last_engagement_time());
    EXPECT_EQ(max_decay_time + service->GetStalePeriod(),
              service->CreateEngagementScore(url4).last_engagement_time());
    EXPECT_EQ(max_decay_time + service->GetStalePeriod(),
              service->GetLastEngagementTime());
  }

  {
    // Add points to commit the decay.
    service->AddPoints(url1, 0.5);
    service->AddPoints(url2, 0.5);
    service->AddPoints(url4, 1);

    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_EQ(3u, score_map.size());
    EXPECT_EQ(9.0, score_map[url1]);
    EXPECT_EQ(2.5, score_map[url2]);
    EXPECT_EQ(3.0, score_map[url4]);
    EXPECT_EQ(clock->Now(),
              service->CreateEngagementScore(url1).last_engagement_time());
    EXPECT_EQ(clock->Now(),
              service->CreateEngagementScore(url2).last_engagement_time());
    EXPECT_EQ(clock->Now(),
              service->CreateEngagementScore(url4).last_engagement_time());
    EXPECT_EQ(clock->Now(), service->GetLastEngagementTime());
  }

  {
    // Advance time by a decay period after the current last engagement time.
    // Expect url2/url4 to be decayed to zero and url1 to decay once.
    base_time = clock->Now() + decay_period;
    clock->SetNow(base_time);
    ASSERT_FALSE(service->IsLastEngagementStale());

    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_EQ(3u, score_map.size());
    EXPECT_EQ(4, score_map[url1]);
    EXPECT_EQ(0, score_map[url2]);
    EXPECT_EQ(0, score_map[url4]);

    service->CleanupEngagementScores(false);
    ASSERT_FALSE(service->IsLastEngagementStale());

    score_map = service->GetScoreMap();
    EXPECT_EQ(1u, score_map.size());
    EXPECT_EQ(4, score_map[url1]);
    EXPECT_EQ(0, service->GetScore(url2));
    EXPECT_EQ(0, service->GetScore(url4));
    EXPECT_EQ(clock->Now() - decay_period,
              service->CreateEngagementScore(url1).last_engagement_time());
    EXPECT_EQ(clock->Now() - decay_period, service->GetLastEngagementTime());
  }

  {
    // Add points to commit the decay.
    service->AddPoints(url1, 0.5);

    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_EQ(1u, score_map.size());
    EXPECT_EQ(4.5, score_map[url1]);
    EXPECT_EQ(clock->Now(),
              service->CreateEngagementScore(url1).last_engagement_time());
    EXPECT_EQ(clock->Now(), service->GetLastEngagementTime());
  }

  {
    // Another decay period will decay url1 to zero.
    clock->SetNow(clock->Now() + decay_period);
    ASSERT_FALSE(service->IsLastEngagementStale());

    std::map<GURL, double> score_map = service->GetScoreMap();
    EXPECT_EQ(1u, score_map.size());
    EXPECT_EQ(0, score_map[url1]);
    EXPECT_EQ(clock->Now() - decay_period,
              service->CreateEngagementScore(url1).last_engagement_time());
    EXPECT_EQ(clock->Now() - decay_period, service->GetLastEngagementTime());

    service->CleanupEngagementScores(false);
    ASSERT_FALSE(service->IsLastEngagementStale());

    score_map = service->GetScoreMap();
    EXPECT_EQ(0u, score_map.size());
    EXPECT_EQ(0, service->GetScore(url1));
    EXPECT_EQ(clock->Now() - decay_period, service->GetLastEngagementTime());
  }
}

TEST_F(SiteEngagementServiceTest, CleanupEngagementScoresProportional) {
  SetParamValue(SiteEngagementScore::DECAY_PROPORTION, 0.5);
  SetParamValue(SiteEngagementScore::DECAY_POINTS, 0);
  SetParamValue(SiteEngagementScore::SCORE_CLEANUP_THRESHOLD, 0.5);

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

  GURL url1("https://www.google.com/");
  GURL url2("https://www.somewhereelse.com/");

  service->AddPoints(url1, 1.0);
  service->AddPoints(url2, 1.2);

  current_day += base::TimeDelta::FromDays(7);
  clock->SetNow(current_day);
  std::map<GURL, double> score_map = service->GetScoreMap();
  EXPECT_EQ(2u, score_map.size());
  AssertInRange(0.5, service->GetScore(url1));
  AssertInRange(0.6, service->GetScore(url2));

  service->CleanupEngagementScores(false);
  score_map = service->GetScoreMap();
  EXPECT_EQ(1u, score_map.size());
  EXPECT_EQ(0, service->GetScore(url1));
  AssertInRange(0.6, service->GetScore(url2));
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

  clock->SetNow(current_day + base::TimeDelta::FromDays(8));
  EXPECT_FALSE(service->IsBootstrapped());
}

TEST_F(SiteEngagementServiceTest, CleanupOriginsOnHistoryDeletion) {
  // Enable proportional decay to ensure that the undecay that happens to
  // balance out history deletion also accounts for the proportional decay.
  SetParamValue(SiteEngagementScore::DECAY_PROPORTION, 0.5);

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

  AssertInRange(3.0, engagement->GetScore(origin1));
  AssertInRange(5.0, engagement->GetScore(origin2));
  AssertInRange(5.0, engagement->GetScore(origin3));
  AssertInRange(5.0, engagement->GetScore(origin4));

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
    AssertInRange(2, engagement->GetScore(origin1));
    EXPECT_EQ(0, engagement->GetScore(origin2));
    AssertInRange(5.0, engagement->GetScore(origin3));
    AssertInRange(2.5, engagement->GetScore(origin4));
    AssertInRange(9.5, engagement->GetTotalEngagementPoints());
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
    AssertInRange(1, engagement->GetScore(origin1));
    EXPECT_EQ(0, engagement->GetScore(origin2));
    AssertInRange(5.0, engagement->GetScore(origin3));
    AssertInRange(2.5, engagement->GetScore(origin4));
    AssertInRange(8.5, engagement->GetTotalEngagementPoints());
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
    AssertInRange(5.0, engagement->GetScore(origin3));
    AssertInRange(2.5, engagement->GetScore(origin4));
    AssertInRange(7.5, engagement->GetTotalEngagementPoints());
  }
}

TEST_F(SiteEngagementServiceTest, EngagementLevel) {
  static_assert(blink::mojom::EngagementLevel::NONE !=
                    blink::mojom::EngagementLevel::LOW,
                "enum values should not be equal");
  static_assert(blink::mojom::EngagementLevel::LOW !=
                    blink::mojom::EngagementLevel::MEDIUM,
                "enum values should not be equal");
  static_assert(blink::mojom::EngagementLevel::MEDIUM !=
                    blink::mojom::EngagementLevel::HIGH,
                "enum values should not be equal");
  static_assert(blink::mojom::EngagementLevel::HIGH !=
                    blink::mojom::EngagementLevel::MAX,
                "enum values should not be equal");

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");

  EXPECT_EQ(blink::mojom::EngagementLevel::NONE,
            service->GetEngagementLevel(url1));
  EXPECT_EQ(blink::mojom::EngagementLevel::NONE,
            service->GetEngagementLevel(url2));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url1, blink::mojom::EngagementLevel::NONE));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, blink::mojom::EngagementLevel::MINIMAL));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url1, blink::mojom::EngagementLevel::LOW));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, blink::mojom::EngagementLevel::MEDIUM));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url1, blink::mojom::EngagementLevel::HIGH));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url1, blink::mojom::EngagementLevel::MAX));

  // Bring url2 to MINIMAL engagement.
  service->AddPoints(url2, 0.5);
  EXPECT_EQ(blink::mojom::EngagementLevel::NONE,
            service->GetEngagementLevel(url1));
  EXPECT_EQ(blink::mojom::EngagementLevel::MINIMAL,
            service->GetEngagementLevel(url2));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::NONE));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, blink::mojom::EngagementLevel::MINIMAL));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::LOW));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url2, blink::mojom::EngagementLevel::MEDIUM));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::HIGH));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::MAX));

  // Bring url1 to LOW engagement.
  service->AddPoints(url1, 1.0);
  EXPECT_EQ(blink::mojom::EngagementLevel::LOW,
            service->GetEngagementLevel(url1));
  EXPECT_EQ(blink::mojom::EngagementLevel::MINIMAL,
            service->GetEngagementLevel(url2));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url1, blink::mojom::EngagementLevel::NONE));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url1, blink::mojom::EngagementLevel::MINIMAL));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url1, blink::mojom::EngagementLevel::LOW));
  EXPECT_FALSE(service->IsEngagementAtLeast(
      url1, blink::mojom::EngagementLevel::MEDIUM));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url1, blink::mojom::EngagementLevel::HIGH));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url1, blink::mojom::EngagementLevel::MAX));

  // Bring url2 to MEDIUM engagement.
  service->AddPoints(url2, 4.5);
  EXPECT_EQ(blink::mojom::EngagementLevel::LOW,
            service->GetEngagementLevel(url1));
  EXPECT_EQ(blink::mojom::EngagementLevel::MEDIUM,
            service->GetEngagementLevel(url2));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::NONE));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, blink::mojom::EngagementLevel::MINIMAL));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::LOW));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, blink::mojom::EngagementLevel::MEDIUM));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::HIGH));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::MAX));

  // Bring url2 to HIGH engagement.
  for (int i = 0; i < 9; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    clock->SetNow(current_day);
    service->AddPoints(url2, 5.0);
  }
  EXPECT_EQ(blink::mojom::EngagementLevel::HIGH,
            service->GetEngagementLevel(url2));

  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::NONE));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, blink::mojom::EngagementLevel::MINIMAL));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::LOW));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, blink::mojom::EngagementLevel::MEDIUM));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::HIGH));
  EXPECT_FALSE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::MAX));

  // Bring url2 to MAX engagement.
  for (int i = 0; i < 10; ++i) {
    current_day += base::TimeDelta::FromDays(1);
    clock->SetNow(current_day);
    service->AddPoints(url2, 5.0);
  }
  EXPECT_EQ(blink::mojom::EngagementLevel::MAX,
            service->GetEngagementLevel(url2));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::NONE));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, blink::mojom::EngagementLevel::MINIMAL));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::LOW));
  EXPECT_TRUE(service->IsEngagementAtLeast(
      url2, blink::mojom::EngagementLevel::MEDIUM));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::HIGH));
  EXPECT_TRUE(
      service->IsEngagementAtLeast(url2, blink::mojom::EngagementLevel::MAX));
}

TEST_F(SiteEngagementServiceTest, Observers) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());

  GURL url_score_1("http://www.google.com/maps");
  GURL url_score_2("http://www.google.com/drive");
  GURL url_score_3("http://www.google.com/");
  GURL url_not_called("https://www.google.com/");

  // Create an observer and Observe(nullptr).
  ObserverTester tester_not_called(service, web_contents(), url_not_called, 1);
  tester_not_called.Observe(nullptr);

  {
    // Create an observer for navigation.
    ObserverTester tester(service, web_contents(), url_score_1, 0.5);
    NavigateAndCommit(url_score_1);
    service->HandleNavigation(web_contents(), ui::PAGE_TRANSITION_TYPED);
    tester.Wait();
    EXPECT_TRUE(tester.callback_called());
    EXPECT_FALSE(tester_not_called.callback_called());
    tester.Observe(nullptr);
  }

  {
    // Update observer for a user input.
    ObserverTester tester(service, web_contents(), url_score_2, 0.55);
    NavigateAndCommit(url_score_2);
    service->HandleUserInput(web_contents(),
                             SiteEngagementMetrics::ENGAGEMENT_MOUSE);
    tester.Wait();
    EXPECT_TRUE(tester.callback_called());
    EXPECT_FALSE(tester_not_called.callback_called());
    tester.Observe(nullptr);
  }

  // Add two observers for media playing in the foreground.
  {
    ObserverTester tester_1(service, web_contents(), url_score_3, 0.57);
    ObserverTester tester_2(service, web_contents(), url_score_3, 0.57);
    NavigateAndCommit(url_score_3);
    service->HandleMediaPlaying(web_contents(), false);
    tester_1.Wait();
    tester_2.Wait();

    EXPECT_TRUE(tester_1.callback_called());
    EXPECT_TRUE(tester_2.callback_called());
    EXPECT_FALSE(tester_not_called.callback_called());
    tester_1.Observe(nullptr);
    tester_2.Observe(nullptr);
  }

  // Add an observer for media playing in the background.
  {
    ObserverTester tester(service, web_contents(), url_score_3, 0.58);
    service->HandleMediaPlaying(web_contents(), true);
    tester.Wait();

    EXPECT_TRUE(tester.callback_called());
    EXPECT_FALSE(tester_not_called.callback_called());
    tester.Observe(nullptr);
  }
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
  clock->SetNow(
      current_day +
      base::TimeDelta::FromHours(SiteEngagementScore::GetDecayPeriodInHours()));

  // Trigger decay and histogram hit.
  service->AddPoints(origin1, 0.01);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kScoreDecayedFromHistogram,
      SiteEngagementScore::kMaxPoints, 1);
  histograms.ExpectUniqueSample(
      SiteEngagementMetrics::kScoreDecayedToHistogram,
      SiteEngagementScore::kMaxPoints - SiteEngagementScore::GetDecayPoints(),
      1);

  // Check histograms after another decay period.
  clock->SetNow(current_day +
                base::TimeDelta::FromHours(
                    2 * SiteEngagementScore::GetDecayPeriodInHours()));
  // Trigger decay and histogram hit.
  service->AddPoints(origin1, 0.01);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              2);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              2);

  // Check decay to zero. Start at the 3rd decay period (we have had two
  // already). This will be 40 decays in total.
  for (int i = 3; i <= kMorePeriodsThanNeededToDecayMaxScore; ++i) {
    clock->SetNow(current_day +
                  base::TimeDelta::FromHours(
                      i * SiteEngagementScore::GetDecayPeriodInHours()));
    // Trigger decay and histogram hit.
    service->AddPoints(origin1, 0.01);
  }
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              kMorePeriodsThanNeededToDecayMaxScore);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              kMorePeriodsThanNeededToDecayMaxScore);
  // It should have taken (20 - 3) = 17 of the 38 decays to get to zero, since
  // we started from 95. Expect the remaining 21 decays to be to bucket 0 (and
  // hence 20 from bucket 0).
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kScoreDecayedFromHistogram, 0, 20);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                               0, 21);
  // Trigger decay and histogram hit for origin2, checking an independent decay.
  service->AddPoints(origin2, 0.01);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              kMorePeriodsThanNeededToDecayMaxScore + 1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              kMorePeriodsThanNeededToDecayMaxScore + 1);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kScoreDecayedFromHistogram, 0, 21);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                               0, 22);

  // Add more points and ensure no more samples are present.
  service->AddPoints(origin1, 0.01);
  service->AddPoints(origin2, 0.01);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedFromHistogram,
                              kMorePeriodsThanNeededToDecayMaxScore + 1);
  histograms.ExpectTotalCount(SiteEngagementMetrics::kScoreDecayedToHistogram,
                              kMorePeriodsThanNeededToDecayMaxScore + 1);
}

TEST_F(SiteEngagementServiceTest, LastEngagementTime) {
  // The last engagement time should start off null in prefs and in the service.
  base::Time last_engagement_time = base::Time::FromInternalValue(
      profile()->GetPrefs()->GetInt64(prefs::kSiteEngagementLastUpdateTime));

  ASSERT_TRUE(last_engagement_time.is_null());

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));

  ASSERT_TRUE(service->GetLastEngagementTime().is_null());

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

  // Add points should set the last engagement time in the service, and persist
  // it to disk.
  GURL origin("http://www.google.com/");
  service->AddPoints(origin, 1);

  last_engagement_time = base::Time::FromInternalValue(
      profile()->GetPrefs()->GetInt64(prefs::kSiteEngagementLastUpdateTime));

  EXPECT_EQ(current_day, service->GetLastEngagementTime());
  EXPECT_EQ(current_day, last_engagement_time);

  // Running a cleanup and updating last engagement times should persist the
  // last engagement time to disk.
  current_day += service->GetStalePeriod();
  base::Time rebased_time = current_day - service->GetMaxDecayPeriod();
  clock->SetNow(current_day);
  service->CleanupEngagementScores(true);

  last_engagement_time = base::Time::FromInternalValue(
      profile()->GetPrefs()->GetInt64(prefs::kSiteEngagementLastUpdateTime));

  EXPECT_EQ(rebased_time, last_engagement_time);
  EXPECT_EQ(rebased_time, service->GetLastEngagementTime());

  // Adding 0 points shouldn't update the last engagement time.
  base::Time later_in_day = current_day + base::TimeDelta::FromSeconds(30);
  clock->SetNow(later_in_day);
  service->AddPoints(origin, 0);

  last_engagement_time = base::Time::FromInternalValue(
      profile()->GetPrefs()->GetInt64(prefs::kSiteEngagementLastUpdateTime));
  EXPECT_EQ(rebased_time, last_engagement_time);
  EXPECT_EQ(rebased_time, service->GetLastEngagementTime());

  // Add some more points and ensure the value is persisted.
  service->AddPoints(origin, 3);

  last_engagement_time = base::Time::FromInternalValue(
      profile()->GetPrefs()->GetInt64(prefs::kSiteEngagementLastUpdateTime));

  EXPECT_EQ(later_in_day, last_engagement_time);
  EXPECT_EQ(later_in_day, service->GetLastEngagementTime());
}

TEST_F(SiteEngagementServiceTest, CleanupMovesScoreBackToNow) {
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));
  base::Time last_engagement_time;

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

  GURL origin("http://www.google.com/");
  service->AddPoints(origin, 1);
  EXPECT_EQ(1, service->GetScore(origin));
  EXPECT_EQ(current_day, service->GetLastEngagementTime());

  // Send the clock back in time before the stale period and add engagement for
  // a new origin. Ensure that the original origin has its last engagement time
  // updated to now as a result.
  base::Time before_stale_period =
      clock->Now() - service->GetStalePeriod() - service->GetMaxDecayPeriod();
  clock->SetNow(before_stale_period);

  GURL origin1("http://maps.google.com/");
  service->AddPoints(origin1, 1);

  EXPECT_EQ(before_stale_period,
            service->CreateEngagementScore(origin).last_engagement_time());
  EXPECT_EQ(before_stale_period, service->GetLastEngagementTime());
  EXPECT_EQ(1, service->GetScore(origin));
  EXPECT_EQ(1, service->GetScore(origin1));

  // Advance within a decay period and add points.
  base::TimeDelta less_than_decay_period =
      base::TimeDelta::FromHours(SiteEngagementScore::GetDecayPeriodInHours()) -
      base::TimeDelta::FromSeconds(30);
  base::Time origin1_last_updated = clock->Now() + less_than_decay_period;
  clock->SetNow(origin1_last_updated);
  service->AddPoints(origin, 1);
  service->AddPoints(origin1, 5);
  EXPECT_EQ(2, service->GetScore(origin));
  EXPECT_EQ(6, service->GetScore(origin1));

  clock->SetNow(clock->Now() + less_than_decay_period);
  service->AddPoints(origin, 5);
  EXPECT_EQ(7, service->GetScore(origin));

  // Move forward to the max number of decays per score. This is within the
  // stale period so no cleanup should be run.
  for (int i = 0; i < SiteEngagementScore::GetMaxDecaysPerScore(); ++i) {
    clock->SetNow(clock->Now() + less_than_decay_period);
    service->AddPoints(origin, 5);
    EXPECT_EQ(clock->Now(), service->GetLastEngagementTime());
  }
  EXPECT_EQ(12, service->GetScore(origin));
  EXPECT_EQ(clock->Now(), service->GetLastEngagementTime());

  // Move the clock back to precisely 1 decay period after origin1's last
  // updated time. |last_engagement_time| is in the future, so AddPoints
  // triggers a cleanup. Ensure that |last_engagement_time| is moved back
  // appropriately, while origin1 is decayed correctly (once).
  clock->SetNow(origin1_last_updated + less_than_decay_period +
                base::TimeDelta::FromSeconds(30));
  service->AddPoints(origin1, 1);

  EXPECT_EQ(clock->Now(),
            service->CreateEngagementScore(origin).last_engagement_time());
  EXPECT_EQ(clock->Now(), service->GetLastEngagementTime());
  EXPECT_EQ(12, service->GetScore(origin));
  EXPECT_EQ(1, service->GetScore(origin1));
}

TEST_F(SiteEngagementServiceTest, CleanupMovesScoreBackToRebase) {
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  std::unique_ptr<SiteEngagementService> service(
      new SiteEngagementService(profile(), base::WrapUnique(clock)));
  base::Time last_engagement_time;

  base::Time current_day = GetReferenceTime();
  clock->SetNow(current_day);

  GURL origin("http://www.google.com/");
  service->ResetScoreForURL(origin, 5);
  service->AddPoints(origin, 5);
  EXPECT_EQ(10, service->GetScore(origin));
  EXPECT_EQ(current_day, service->GetLastEngagementTime());

  // Send the clock back in time before the stale period and add engagement for
  // a new origin.
  base::Time before_stale_period =
      clock->Now() - service->GetStalePeriod() - service->GetMaxDecayPeriod();
  clock->SetNow(before_stale_period);

  GURL origin1("http://maps.google.com/");
  service->AddPoints(origin1, 1);

  EXPECT_EQ(before_stale_period, service->GetLastEngagementTime());

  // Set the clock such that |origin|'s last engagement time is between
  // last_engagement_time and rebase_time.
  clock->SetNow(current_day + service->GetStalePeriod() +
                service->GetMaxDecayPeriod() -
                base::TimeDelta::FromSeconds((30)));
  base::Time rebased_time = clock->Now() - service->GetMaxDecayPeriod();
  service->CleanupEngagementScores(true);

  // Ensure that the original origin has its last engagement time updated to
  // rebase_time, and it has decayed when we access the score.
  EXPECT_EQ(rebased_time,
            service->CreateEngagementScore(origin).last_engagement_time());
  EXPECT_EQ(rebased_time, service->GetLastEngagementTime());
  EXPECT_EQ(5, service->GetScore(origin));
  EXPECT_EQ(0, service->GetScore(origin1));
}

TEST_F(SiteEngagementServiceTest, IncognitoEngagementService) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  GURL url1("http://www.google.com/");
  GURL url2("https://www.google.com/");
  GURL url3("https://drive.google.com/");
  GURL url4("https://maps.google.com/");

  service->AddPoints(url1, 1);
  service->AddPoints(url2, 2);

  SiteEngagementService* incognito_service =
      SiteEngagementService::Get(profile()->GetOffTheRecordProfile());
  EXPECT_EQ(1, incognito_service->GetScore(url1));
  EXPECT_EQ(2, incognito_service->GetScore(url2));
  EXPECT_EQ(0, incognito_service->GetScore(url3));

  incognito_service->AddPoints(url3, 1);
  EXPECT_EQ(1, incognito_service->GetScore(url3));
  EXPECT_EQ(0, service->GetScore(url3));

  incognito_service->AddPoints(url2, 1);
  EXPECT_EQ(3, incognito_service->GetScore(url2));
  EXPECT_EQ(2, service->GetScore(url2));

  service->AddPoints(url3, 2);
  EXPECT_EQ(1, incognito_service->GetScore(url3));
  EXPECT_EQ(2, service->GetScore(url3));

  EXPECT_EQ(0, incognito_service->GetScore(url4));
  service->AddPoints(url4, 2);
  EXPECT_EQ(2, incognito_service->GetScore(url4));
  EXPECT_EQ(2, service->GetScore(url4));
}

TEST_F(SiteEngagementServiceTest, GetScoreFromSettings) {
  GURL url1("http://www.google.com/");
  GURL url2("https://www.google.com/");

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  HostContentSettingsMap* incognito_settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          profile()->GetOffTheRecordProfile());

  // All scores are 0 to start.
  EXPECT_EQ(0, CheckScoreFromSettingsOnThread(content::BrowserThread::IO,
                                              settings_map, url1));
  EXPECT_EQ(0, CheckScoreFromSettingsOnThread(content::BrowserThread::FILE,
                                              settings_map, url2));
  EXPECT_EQ(0, CheckScoreFromSettingsOnThread(content::BrowserThread::FILE,
                                              incognito_settings_map, url1));
  EXPECT_EQ(0, CheckScoreFromSettingsOnThread(content::BrowserThread::IO,
                                              incognito_settings_map, url2));

  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);
  service->AddPoints(url1, 1);
  service->AddPoints(url2, 2);

  EXPECT_EQ(1, CheckScoreFromSettingsOnThread(content::BrowserThread::FILE,
                                              settings_map, url1));
  EXPECT_EQ(2, CheckScoreFromSettingsOnThread(content::BrowserThread::IO,
                                              settings_map, url2));
  EXPECT_EQ(1, CheckScoreFromSettingsOnThread(content::BrowserThread::FILE,
                                              incognito_settings_map, url1));
  EXPECT_EQ(2, CheckScoreFromSettingsOnThread(content::BrowserThread::IO,
                                              incognito_settings_map, url2));

  SiteEngagementService* incognito_service =
      SiteEngagementService::Get(profile()->GetOffTheRecordProfile());
  ASSERT_TRUE(incognito_service);
  incognito_service->AddPoints(url1, 3);
  incognito_service->AddPoints(url2, 1);

  EXPECT_EQ(1, CheckScoreFromSettingsOnThread(content::BrowserThread::IO,
                                              settings_map, url1));
  EXPECT_EQ(2, CheckScoreFromSettingsOnThread(content::BrowserThread::FILE,
                                              settings_map, url2));
  EXPECT_EQ(4, CheckScoreFromSettingsOnThread(content::BrowserThread::IO,
                                              incognito_settings_map, url1));
  EXPECT_EQ(3, CheckScoreFromSettingsOnThread(content::BrowserThread::FILE,
                                              incognito_settings_map, url2));

  service->AddPoints(url1, 2);
  service->AddPoints(url2, 1);

  EXPECT_EQ(3, CheckScoreFromSettingsOnThread(content::BrowserThread::IO,
                                              settings_map, url1));
  EXPECT_EQ(3, CheckScoreFromSettingsOnThread(content::BrowserThread::FILE,
                                              settings_map, url2));
  EXPECT_EQ(4, CheckScoreFromSettingsOnThread(content::BrowserThread::FILE,
                                              incognito_settings_map, url1));
  EXPECT_EQ(3, CheckScoreFromSettingsOnThread(content::BrowserThread::IO,
                                              incognito_settings_map, url2));
}
