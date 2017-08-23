// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_score.h"

#include <utility>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

class MediaEngagementScoreTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitFromCommandLine("RecordMediaEngagementScores",
                                             std::string());

    ChromeRenderViewHostTestHarness::SetUp();
    test_clock.SetNow(GetReferenceTime());
    score_ = new MediaEngagementScore(&test_clock, GURL(), nullptr);
  }

  void TearDown() override {
    delete score_;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  base::SimpleTestClock test_clock;

 protected:
  MediaEngagementScore* score_;

  void VerifyScore(MediaEngagementScore* score,
                   int expected_visits,
                   int expected_media_playbacks,
                   base::Time expected_last_media_playback_time,
                   bool has_high_score) {
    EXPECT_EQ(expected_visits, score->visits());
    EXPECT_EQ(expected_media_playbacks, score->media_playbacks());
    EXPECT_EQ(expected_last_media_playback_time,
              score->last_media_playback_time());
    EXPECT_EQ(has_high_score, score->high_score());
  }

  void UpdateScore(MediaEngagementScore* score) {
    test_clock.SetNow(test_clock.Now() + base::TimeDelta::FromHours(1));

    score->IncrementVisits();
    score->IncrementMediaPlaybacks();
  }

  void TestScoreInitializesAndUpdates(
      std::unique_ptr<base::DictionaryValue> score_dict,
      int expected_visits,
      int expected_media_playbacks,
      base::Time expected_last_media_playback_time,
      bool has_high_score) {
    MediaEngagementScore* initial_score =
        new MediaEngagementScore(&test_clock, GURL(), std::move(score_dict));
    VerifyScore(initial_score, expected_visits, expected_media_playbacks,
                expected_last_media_playback_time, has_high_score);

    // Updating the score dict should return false, as the score shouldn't
    // have changed at this point.
    EXPECT_FALSE(initial_score->UpdateScoreDict());

    // Increment the scores and check that the values were stored correctly.
    UpdateScore(initial_score);
    EXPECT_TRUE(initial_score->UpdateScoreDict());
    delete initial_score;
  }

  void SetScore(int visits, int media_playbacks) {
    score_->SetVisits(visits);
    score_->SetMediaPlaybacks(media_playbacks);
  }

  void VerifyGetScoreDetails(MediaEngagementScore* score) {
    media::mojom::MediaEngagementScoreDetailsPtr details =
        score->GetScoreDetails();
    EXPECT_EQ(details->origin, score->origin_);
    EXPECT_EQ(details->total_score, score->actual_score());
    EXPECT_EQ(details->visits, score->visits());
    EXPECT_EQ(details->media_playbacks, score->media_playbacks());
    EXPECT_EQ(details->last_media_playback_time,
              score->last_media_playback_time().ToJsTime());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test Mojo serialization.
TEST_F(MediaEngagementScoreTest, MojoSerialization) {
  VerifyGetScoreDetails(score_);
  UpdateScore(score_);
  VerifyGetScoreDetails(score_);
}

// Test that scores are read / written correctly from / to empty score
// dictionaries.
TEST_F(MediaEngagementScoreTest, EmptyDictionary) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  TestScoreInitializesAndUpdates(std::move(dict), 0, 0, base::Time(), false);
}

// Test that scores are read / written correctly from / to partially empty
// score dictionaries.
TEST_F(MediaEngagementScoreTest, PartiallyEmptyDictionary) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger(MediaEngagementScore::kVisitsKey, 2);

  TestScoreInitializesAndUpdates(std::move(dict), 2, 0, base::Time(), false);
}

// Test that scores are read / written correctly from / to populated score
// dictionaries.
TEST_F(MediaEngagementScoreTest, PopulatedDictionary) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger(MediaEngagementScore::kVisitsKey, 10);
  dict->SetInteger(MediaEngagementScore::kMediaPlaybacksKey, 6);
  dict->SetDouble(MediaEngagementScore::kLastMediaPlaybackTimeKey,
                  test_clock.Now().ToInternalValue());
  dict->SetBoolean(MediaEngagementScore::kHasHighScoreKey, true);

  TestScoreInitializesAndUpdates(std::move(dict), 10, 6, test_clock.Now(),
                                 true);
}

// Test getting and commiting the score works correctly with different
// origins.
TEST_F(MediaEngagementScoreTest, ContentSettingsMultiOrigin) {
  GURL url("https://www.google.com");

  // Replace |score_| with one with an actual URL, and with a settings map.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  MediaEngagementScore* score =
      new MediaEngagementScore(&test_clock, url, settings_map);

  // Verify the score is originally zero, try incrementing and storing
  // the score.
  VerifyScore(score, 0, 0, base::Time(), false);
  score->IncrementVisits();
  UpdateScore(score);
  score->Commit();

  // Now confirm the correct score is present on the same origin,
  // but zero for a different origin.
  GURL same_origin("https://www.google.com");
  GURL different_origin("https://www.google.co.uk");
  MediaEngagementScore* new_score =
      new MediaEngagementScore(&test_clock, url, settings_map);
  MediaEngagementScore* same_origin_score =
      new MediaEngagementScore(&test_clock, same_origin, settings_map);
  MediaEngagementScore* different_origin_score =
      new MediaEngagementScore(&test_clock, different_origin, settings_map);
  VerifyScore(new_score, 2, 1, test_clock.Now(), false);
  VerifyScore(same_origin_score, 2, 1, test_clock.Now(), false);
  VerifyScore(different_origin_score, 0, 0, base::Time(), false);

  delete score;
  delete new_score;
  delete same_origin_score;
  delete different_origin_score;
}

// Tests content settings read/write.
TEST_F(MediaEngagementScoreTest, ContentSettings) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  int example_num_visits = 10;
  int example_media_playbacks = 6;

  // Store some example data in content settings.
  GURL origin("https://www.google.com");
  std::unique_ptr<base::DictionaryValue> score_dict =
      base::MakeUnique<base::DictionaryValue>();
  score_dict->SetInteger(MediaEngagementScore::kVisitsKey, example_num_visits);
  score_dict->SetInteger(MediaEngagementScore::kMediaPlaybacksKey,
                         example_media_playbacks);
  score_dict->SetDouble(MediaEngagementScore::kLastMediaPlaybackTimeKey,
                        test_clock.Now().ToInternalValue());
  score_dict->SetBoolean(MediaEngagementScore::kHasHighScoreKey, false);
  settings_map->SetWebsiteSettingDefaultScope(
      origin, GURL(), CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT,
      content_settings::ResourceIdentifier(), std::move(score_dict));

  // Make sure we read that data back correctly.
  MediaEngagementScore* score =
      new MediaEngagementScore(&test_clock, origin, settings_map);
  EXPECT_EQ(score->visits(), example_num_visits);
  EXPECT_EQ(score->media_playbacks(), example_media_playbacks);
  EXPECT_EQ(score->last_media_playback_time(), test_clock.Now());
  EXPECT_FALSE(score->high_score());

  UpdateScore(score);
  score->IncrementMediaPlaybacks();
  EXPECT_TRUE(score->high_score());
  score->Commit();

  // Now read back content settings and make sure we have the right values.
  int stored_visits;
  int stored_media_playbacks;
  double stored_last_media_playback_time;
  bool stored_has_high_score;
  std::unique_ptr<base::DictionaryValue> values =
      base::DictionaryValue::From(settings_map->GetWebsiteSetting(
          origin, GURL(), CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT,
          content_settings::ResourceIdentifier(), nullptr));
  values->GetInteger(MediaEngagementScore::kVisitsKey, &stored_visits);
  values->GetInteger(MediaEngagementScore::kMediaPlaybacksKey,
                     &stored_media_playbacks);
  values->GetDouble(MediaEngagementScore::kLastMediaPlaybackTimeKey,
                    &stored_last_media_playback_time);
  values->GetBoolean(MediaEngagementScore::kHasHighScoreKey,
                     &stored_has_high_score);
  EXPECT_EQ(stored_visits, example_num_visits + 1);
  EXPECT_EQ(stored_media_playbacks, example_media_playbacks + 2);
  EXPECT_EQ(stored_last_media_playback_time,
            test_clock.Now().ToInternalValue());
  EXPECT_TRUE(stored_has_high_score);

  delete score;
}

// Test that the engagement score is calculated correctly.
TEST_F(MediaEngagementScoreTest, EngagementScoreCalculation) {
  EXPECT_EQ(0, score_->actual_score());
  UpdateScore(score_);

  // Check that the score is zero even with 1 visit.
  EXPECT_EQ(0.0, score_->actual_score());

  EXPECT_EQ(0, score_->actual_score());
  SetScore(10, 4);
  EXPECT_EQ(0.4, score_->actual_score());

  UpdateScore(score_);
  EXPECT_EQ(5.0 / 11.0, score_->actual_score());
}

// Test that a score without the high_score bit uses the correct bounds.
TEST_F(MediaEngagementScoreTest, HighScoreLegacy) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger(MediaEngagementScore::kVisitsKey, 10);
  dict->SetInteger(MediaEngagementScore::kMediaPlaybacksKey, 7);
  TestScoreInitializesAndUpdates(std::move(dict), 10, 7, base::Time(), true);

  std::unique_ptr<base::DictionaryValue> dict2(new base::DictionaryValue());
  dict2->SetInteger(MediaEngagementScore::kVisitsKey, 10);
  dict2->SetInteger(MediaEngagementScore::kMediaPlaybacksKey, 4);
  TestScoreInitializesAndUpdates(std::move(dict2), 10, 4, base::Time(), false);
}

// Test that if we changed the boundaries the high_score bit is updated
// when the score is loaded.
TEST_F(MediaEngagementScoreTest, HighScoreUpdated) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger(MediaEngagementScore::kVisitsKey, 10);
  dict->SetInteger(MediaEngagementScore::kMediaPlaybacksKey, 4);
  dict->SetDouble(MediaEngagementScore::kLastMediaPlaybackTimeKey,
                  test_clock.Now().ToInternalValue());
  dict->SetBoolean(MediaEngagementScore::kHasHighScoreKey, true);

  TestScoreInitializesAndUpdates(std::move(dict), 10, 4, test_clock.Now(),
                                 false);
}

// Test that the has high score upper and lower thresholds work.
TEST_F(MediaEngagementScoreTest, HighScoreThreshold) {
  EXPECT_FALSE(score_->high_score());

  // Test that a total score of 0.1 is not high.
  SetScore(10, 1);
  EXPECT_FALSE(score_->high_score());

  // Test that a total score of 0.6 is not high but above zero.
  SetScore(10, 6);
  EXPECT_FALSE(score_->high_score());

  // Test that a total score of 0.8 is high.
  SetScore(10, 8);
  EXPECT_TRUE(score_->high_score());

  // Test that a total score of 0.5 is high because of the lower boundary.
  SetScore(10, 5);
  EXPECT_TRUE(score_->high_score());

  // Test that a total score of 0.1 is not high.
  SetScore(10, 1);
  EXPECT_FALSE(score_->high_score());
}
