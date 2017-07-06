// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_service.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
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

}  // namespace

class MediaEngagementServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(media::kMediaEngagement);
    ChromeRenderViewHostTestHarness::SetUp();

    test_clock_ = new base::SimpleTestClock();
    test_clock_->SetNow(GetReferenceTime());
    service_ = base::WrapUnique(
        new MediaEngagementService(profile(), base::WrapUnique(test_clock_)));
  }

  void RecordVisitAndPlaybackAndAdvanceClock(GURL url) {
    RecordVisit(url);
    AdvanceClock();
    RecordPlayback(url);
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

    MediaEngagementScore* score = service->CreateEngagementScore(url);
    EXPECT_EQ(score->visits(), expected_visits);
    EXPECT_EQ(score->media_playbacks(), expected_media_playbacks);
    EXPECT_EQ(score->last_media_playback_time(),
              expected_last_media_playback_time);
    delete score;
  }

  void ExpectScores(GURL url,
                    double expected_score,
                    int expected_visits,
                    int expected_media_playbacks,
                    base::Time expected_last_media_playback_time) {
    ExpectScores(service_.get(), url, expected_score, expected_visits,
                 expected_media_playbacks, expected_last_media_playback_time);
  }

  base::Time Now() const { return test_clock_->Now(); }

  base::Time TimeNotSet() const { return base::Time(); }

 private:
  base::SimpleTestClock* test_clock_ = nullptr;
  std::unique_ptr<MediaEngagementService> service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

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
