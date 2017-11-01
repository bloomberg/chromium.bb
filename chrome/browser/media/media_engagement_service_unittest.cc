// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_service.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath g_temp_history_dir;

// Waits until a change is observed in media engagement content settings.
class MediaEngagementChangeWaiter : public content_settings::Observer {
 public:
  explicit MediaEngagementChangeWaiter(Profile* profile) : profile_(profile) {
    HostContentSettingsMapFactory::GetForProfile(profile)->AddObserver(this);
  }

  ~MediaEngagementChangeWaiter() override {
    HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(
        this);
  }

  // Overridden from content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override {
    if (content_type == CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT)
      Proceed();
  }

  void Wait() { run_loop_.Run(); }

 private:
  void Proceed() { run_loop_.Quit(); }

  Profile* profile_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementChangeWaiter);
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

class MediaEngagementServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        media::kRecordMediaEngagementScores);
    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    g_temp_history_dir = temp_dir_.GetPath();
    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile(), &BuildTestHistoryService);

    test_clock_ = new base::SimpleTestClock();
    test_clock_->SetNow(GetReferenceTime());
    service_ = base::WrapUnique(
        new MediaEngagementService(profile(), base::WrapUnique(test_clock_)));
  }

  MediaEngagementService* StartNewMediaEngagementService() {
    return MediaEngagementService::Get(profile());
  }

  void RecordVisitAndPlaybackAndAdvanceClock(GURL url) {
    RecordVisit(url);
    AdvanceClock();
    RecordPlayback(url);
  }

  void TearDown() override {
    service_->Shutdown();
    service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void AdvanceClock() {
    test_clock_->SetNow(Now() + base::TimeDelta::FromHours(1));
  }

  void RecordVisit(GURL url) { service_->RecordVisit(url); }

  void RecordPlayback(GURL url) { service_->RecordPlayback(url); }

  void ExpectScores(MediaEngagementService* service,
                    GURL url,
                    double expected_score,
                    int expected_visits,
                    int expected_media_playbacks,
                    base::Time expected_last_media_playback_time) {
    EXPECT_EQ(service->GetEngagementScore(url), expected_score);
    EXPECT_EQ(service->GetScoreMapForTesting()[url], expected_score);

    MediaEngagementScore score = service->CreateEngagementScore(url);
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_media_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_last_media_playback_time,
              score.last_media_playback_time());
  }

  void ExpectScores(GURL url,
                    double expected_score,
                    int expected_visits,
                    int expected_media_playbacks,
                    base::Time expected_last_media_playback_time) {
    ExpectScores(service_.get(), url, expected_score, expected_visits,
                 expected_media_playbacks, expected_last_media_playback_time);
  }

  void SetScores(GURL url, int visits, int media_playbacks) {
    MediaEngagementScore score = service_->CreateEngagementScore(url);
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
    score.Commit();
  }

  void SetLastMediaPlaybackTime(const GURL& url,
                                base::Time last_media_playback_time) {
    MediaEngagementScore score = service_->CreateEngagementScore(url);
    score.last_media_playback_time_ = last_media_playback_time;
    score.Commit();
  }

  double GetActualScore(GURL url) {
    return service_->CreateEngagementScore(url).actual_score();
  }

  std::map<GURL, double> GetScoreMapForTesting() const {
    return service_->GetScoreMapForTesting();
  }

  void ClearDataBetweenTime(base::Time begin, base::Time end) {
    service_->ClearDataBetweenTime(begin, end);
  }

  base::Time Now() const { return test_clock_->Now(); }

  base::Time TimeNotSet() const { return base::Time(); }

  void SetNow(base::Time now) { test_clock_->SetNow(now); }

  std::vector<media::mojom::MediaEngagementScoreDetailsPtr> GetAllScoreDetails()
      const {
    return service_->GetAllScoreDetails();
  }

  bool HasHighEngagement(const GURL& url) const {
    return service_->HasHighEngagement(url);
  }

  void SetSchemaVersion(int version) { service_->SetSchemaVersion(version); }

 private:
  base::SimpleTestClock* test_clock_ = nullptr;

  std::unique_ptr<MediaEngagementService> service_;

  base::ScopedTempDir temp_dir_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MediaEngagementServiceTest, MojoSerialization) {
  EXPECT_EQ(0u, GetAllScoreDetails().size());

  RecordVisitAndPlaybackAndAdvanceClock(GURL("https://www.google.com"));
  EXPECT_EQ(1u, GetAllScoreDetails().size());
}

TEST_F(MediaEngagementServiceTest, RestrictedToHTTPAndHTTPS) {
  GURL url1("ftp://www.google.com/");
  GURL url2("file://blah");
  GURL url3("chrome://");
  GURL url4("about://config");

  RecordVisitAndPlaybackAndAdvanceClock(url1);
  ExpectScores(url1, 0.0, 0, 0, TimeNotSet());

  RecordVisitAndPlaybackAndAdvanceClock(url2);
  ExpectScores(url2, 0.0, 0, 0, TimeNotSet());

  RecordVisitAndPlaybackAndAdvanceClock(url3);
  ExpectScores(url3, 0.0, 0, 0, TimeNotSet());

  RecordVisitAndPlaybackAndAdvanceClock(url4);
  ExpectScores(url4, 0.0, 0, 0, TimeNotSet());
}

TEST_F(MediaEngagementServiceTest,
       HandleRecordVisitAndPlaybackAndAdvanceClockion) {
  GURL url1("https://www.google.com");
  ExpectScores(url1, 0.0, 0, 0, TimeNotSet());
  RecordVisitAndPlaybackAndAdvanceClock(url1);
  ExpectScores(url1, 0.0, 1, 1, Now());

  RecordVisit(url1);
  ExpectScores(url1, 0.0, 2, 1, Now());

  RecordPlayback(url1);
  ExpectScores(url1, 0.0, 2, 2, Now());
  base::Time url1_time = Now();

  GURL url2("https://www.google.co.uk");
  RecordVisitAndPlaybackAndAdvanceClock(url2);
  ExpectScores(url2, 0.0, 1, 1, Now());
  ExpectScores(url1, 0.0, 2, 2, url1_time);
}

TEST_F(MediaEngagementServiceTest, IncognitoEngagementService) {
  GURL url1("http://www.google.com/");
  GURL url2("https://www.google.com/");
  GURL url3("https://drive.google.com/");
  GURL url4("https://maps.google.com/");

  RecordVisitAndPlaybackAndAdvanceClock(url1);
  base::Time url1_time = Now();
  RecordVisitAndPlaybackAndAdvanceClock(url2);

  MediaEngagementService* incognito_service =
      MediaEngagementService::Get(profile()->GetOffTheRecordProfile());
  ExpectScores(incognito_service, url1, 0.0, 1, 1, url1_time);
  ExpectScores(incognito_service, url2, 0.0, 1, 1, Now());
  ExpectScores(incognito_service, url3, 0.0, 0, 0, TimeNotSet());

  incognito_service->RecordVisit(url3);
  ExpectScores(incognito_service, url3, 0.0, 1, 0, TimeNotSet());
  ExpectScores(url3, 0.0, 0, 0, TimeNotSet());

  incognito_service->RecordVisit(url2);
  ExpectScores(incognito_service, url2, 0.0, 2, 1, Now());
  ExpectScores(url2, 0.0, 1, 1, Now());

  RecordVisitAndPlaybackAndAdvanceClock(url3);
  ExpectScores(incognito_service, url3, 0.0, 1, 0, TimeNotSet());
  ExpectScores(url3, 0.0, 1, 1, Now());

  ExpectScores(incognito_service, url4, 0.0, 0, 0, TimeNotSet());
  RecordVisitAndPlaybackAndAdvanceClock(url4);
  ExpectScores(incognito_service, url4, 0.0, 1, 1, Now());
  ExpectScores(url4, 0.0, 1, 1, Now());
}

TEST_F(MediaEngagementServiceTest, CleanupOriginsOnHistoryDeletion) {
  GURL origin1("http://www.google.com/");
  GURL origin1a("http://www.google.com/search?q=asdf");
  GURL origin1b("http://www.google.com/maps/search?q=asdf");
  GURL origin2("https://drive.google.com/");
  GURL origin3("http://deleted.com/");
  GURL origin3a("http://deleted.com/test");
  GURL origin4("http://notdeleted.com");

  // origin1 will have a score that is high enough to not return zero
  // and we will ensure it has the same score. origin2 will have a score
  // that is zero and will remain zero. origin3 will have a score
  // and will be cleared. origin4 will have a normal score.
  SetScores(origin1, MediaEngagementScore::GetScoreMinVisits() + 3, 2);
  SetScores(origin2, 2, 1);
  SetScores(origin3, 2, 1);
  SetScores(origin4, MediaEngagementScore::GetScoreMinVisits(), 2);

  base::Time today = GetReferenceTime();
  base::Time yesterday = GetReferenceTime() - base::TimeDelta::FromDays(1);
  base::Time yesterday_afternoon = GetReferenceTime() -
                                   base::TimeDelta::FromDays(1) +
                                   base::TimeDelta::FromHours(4);
  base::Time yesterday_week = GetReferenceTime() - base::TimeDelta::FromDays(8);
  SetNow(today);

  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);

  history->AddPage(origin1, yesterday_afternoon, history::SOURCE_BROWSED);
  history->AddPage(origin1a, yesterday_afternoon, history::SOURCE_BROWSED);
  history->AddPage(origin1b, yesterday_week, history::SOURCE_BROWSED);
  history->AddPage(origin2, yesterday_afternoon, history::SOURCE_BROWSED);
  history->AddPage(origin3, yesterday_week, history::SOURCE_BROWSED);
  history->AddPage(origin3a, yesterday_afternoon, history::SOURCE_BROWSED);

  // Check that the scores are valid at the beginning.
  ExpectScores(origin1, 0.25, MediaEngagementScore::GetScoreMinVisits() + 3, 2,
               TimeNotSet());
  EXPECT_TRUE(GetActualScore(origin1));
  ExpectScores(origin2, 0.0, 2, 1, TimeNotSet());
  EXPECT_FALSE(GetActualScore(origin2));
  ExpectScores(origin3, 0.0, 2, 1, TimeNotSet());
  EXPECT_FALSE(GetActualScore(origin3));
  ExpectScores(origin4, 0.4, MediaEngagementScore::GetScoreMinVisits(), 2,
               TimeNotSet());
  EXPECT_TRUE(GetActualScore(origin4));

  {
    base::HistogramTester histogram_tester;
    MediaEngagementChangeWaiter waiter(profile());

    base::CancelableTaskTracker task_tracker;
    // Expire origin1, origin1a, origin2, and origin3a's most recent visit.
    history->ExpireHistoryBetween(std::set<GURL>(), yesterday, today,
                                  base::Bind(&base::DoNothing), &task_tracker);
    waiter.Wait();

    // origin1 should have a score that is not zero and is the same as the old
    // score (sometimes it may not match exactly due to rounding). origin2
    // should have a score that is zero but it's visits and playbacks should
    // have decreased. origin3 should have had a decrease in the number of
    // visits. origin4 should have the old score.
    ExpectScores(origin1, 1.0 / 6.0,
                 MediaEngagementScore::GetScoreMinVisits() + 1, 1,
                 TimeNotSet());
    EXPECT_TRUE(GetActualScore(origin1));
    ExpectScores(origin2, 0.0, 1, 0, TimeNotSet());
    EXPECT_FALSE(GetActualScore(origin2));
    ExpectScores(origin3, 0.0, 1, 0, TimeNotSet());
    ExpectScores(origin4, 0.4, MediaEngagementScore::GetScoreMinVisits(), 2,
                 TimeNotSet());

    histogram_tester.ExpectTotalCount(
        MediaEngagementService::kHistogramURLsDeletedScoreReductionName, 3);
    histogram_tester.ExpectBucketCount(
        MediaEngagementService::kHistogramURLsDeletedScoreReductionName, 0, 2);
    histogram_tester.ExpectBucketCount(
        MediaEngagementService::kHistogramURLsDeletedScoreReductionName, 8, 1);
  }

  {
    base::HistogramTester histogram_tester;
    MediaEngagementChangeWaiter waiter(profile());

    // Expire origin1b.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(origin1b);
    args.SetTimeRangeForOneDay(yesterday_week);
    expire_list.push_back(args);

    base::CancelableTaskTracker task_tracker;
    history->ExpireHistory(expire_list, base::Bind(&base::DoNothing),
                           &task_tracker);
    waiter.Wait();

    // origin1's score should have changed but the rest should remain the same.
    ExpectScores(origin1, 0.0, MediaEngagementScore::GetScoreMinVisits(), 0,
                 TimeNotSet());
    ExpectScores(origin2, 0.0, 1, 0, TimeNotSet());
    ExpectScores(origin3, 0.0, 1, 0, TimeNotSet());
    ExpectScores(origin4, 0.4, MediaEngagementScore::GetScoreMinVisits(), 2,
                 TimeNotSet());

    histogram_tester.ExpectTotalCount(
        MediaEngagementService::kHistogramURLsDeletedScoreReductionName, 1);
    histogram_tester.ExpectBucketCount(
        MediaEngagementService::kHistogramURLsDeletedScoreReductionName, 17, 1);
  }

  {
    base::HistogramTester histogram_tester;
    MediaEngagementChangeWaiter waiter(profile());

    // Expire origin3.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(origin3);
    args.SetTimeRangeForOneDay(yesterday_week);
    expire_list.push_back(args);

    base::CancelableTaskTracker task_tracker;
    history->ExpireHistory(expire_list, base::Bind(&base::DoNothing),
                           &task_tracker);
    waiter.Wait();

    // origin3's score should be removed but the rest should remain the same.
    std::map<GURL, double> scores = GetScoreMapForTesting();
    EXPECT_TRUE(scores.find(origin3) == scores.end());
    ExpectScores(origin1, 0.0, MediaEngagementScore::GetScoreMinVisits(), 0,
                 TimeNotSet());
    ExpectScores(origin2, 0.0, 1, 0, TimeNotSet());
    ExpectScores(origin3, 0.0, 0, 0, TimeNotSet());
    ExpectScores(origin4, 0.4, MediaEngagementScore::GetScoreMinVisits(), 2,
                 TimeNotSet());

    histogram_tester.ExpectTotalCount(
        MediaEngagementService::kHistogramURLsDeletedScoreReductionName, 1);
    histogram_tester.ExpectBucketCount(
        MediaEngagementService::kHistogramURLsDeletedScoreReductionName, 0, 1);
  }
}

TEST_F(MediaEngagementServiceTest,
       CleanupDataOnSiteDataCleanup_OutsideBoundary) {
  GURL origin("https://www.google.com");

  base::Time today = GetReferenceTime();
  SetNow(today);

  SetScores(origin, 1, 1);
  SetLastMediaPlaybackTime(origin, today);

  ClearDataBetweenTime(today - base::TimeDelta::FromDays(2),
                       today - base::TimeDelta::FromDays(1));
  ExpectScores(origin, 0.0, 1, 1, today);
}

TEST_F(MediaEngagementServiceTest,
       CleanupDataOnSiteDataCleanup_WithinBoundary) {
  GURL origin1("https://www.google.com");
  GURL origin2("https://www.google.co.uk");

  base::Time today = GetReferenceTime();
  base::Time yesterday = today - base::TimeDelta::FromDays(1);
  base::Time two_days_ago = today - base::TimeDelta::FromDays(2);
  SetNow(today);

  SetScores(origin1, 1, 1);
  SetScores(origin2, 1, 1);
  SetLastMediaPlaybackTime(origin1, yesterday);
  SetLastMediaPlaybackTime(origin2, two_days_ago);

  ClearDataBetweenTime(two_days_ago, yesterday);
  ExpectScores(origin1, 0.0, 0, 0, TimeNotSet());
  ExpectScores(origin2, 0.0, 0, 0, TimeNotSet());
}

TEST_F(MediaEngagementServiceTest, CleanupDataOnSiteDataCleanup_NoTimeSet) {
  GURL origin("https://www.google.com");

  base::Time today = GetReferenceTime();

  SetNow(GetReferenceTime());
  SetScores(origin, 1, 0);

  ClearDataBetweenTime(today - base::TimeDelta::FromDays(2),
                       today - base::TimeDelta::FromDays(1));
  ExpectScores(origin, 0.0, 1, 0, TimeNotSet());
}

TEST_F(MediaEngagementServiceTest, LogScoresOnStartupToHistogram) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.google.co.uk");
  GURL url3("https://www.example.com");

  SetScores(url1, 6, 5);
  SetScores(url2, 6, 3);
  RecordVisitAndPlaybackAndAdvanceClock(url3);
  ExpectScores(url1, 5.0 / 6.0, 6, 5, TimeNotSet());
  ExpectScores(url2, 0.5, 6, 3, TimeNotSet());
  ExpectScores(url3, 0.0, 1, 1, Now());

  base::HistogramTester histogram_tester;
  StartNewMediaEngagementService();

  histogram_tester.ExpectTotalCount(
      MediaEngagementService::kHistogramScoreAtStartupName, 3);
  histogram_tester.ExpectBucketCount(
      MediaEngagementService::kHistogramScoreAtStartupName, 0, 1);
  histogram_tester.ExpectBucketCount(
      MediaEngagementService::kHistogramScoreAtStartupName, 50, 1);
  histogram_tester.ExpectBucketCount(
      MediaEngagementService::kHistogramScoreAtStartupName, 83, 1);
}

TEST_F(MediaEngagementServiceTest, HasHighEngagement) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.google.co.uk");
  GURL url3("https://www.example.com");

  SetScores(url1, 6, 5);
  SetScores(url2, 6, 3);

  EXPECT_TRUE(HasHighEngagement(url1));
  EXPECT_FALSE(HasHighEngagement(url2));
  EXPECT_FALSE(HasHighEngagement(url3));
}

TEST_F(MediaEngagementServiceTest, SchemaVersion_Changed) {
  GURL url("https://www.google.com");
  SetScores(url, 1, 2);

  SetSchemaVersion(0);
  MediaEngagementService* service = StartNewMediaEngagementService();
  ExpectScores(service, url, 0.0, 0, 0, TimeNotSet());
}

TEST_F(MediaEngagementServiceTest, SchemaVersion_Same) {
  GURL url("https://www.google.com");
  SetScores(url, 1, 2);

  MediaEngagementService* service = StartNewMediaEngagementService();
  ExpectScores(service, url, 0.0, 1, 2, TimeNotSet());
}
