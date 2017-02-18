// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ranker.h"

#include <initializer_list>
#include <memory>

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/proto/translate_event.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/proto/translate_ranker_model.pb.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

class TranslateRankerTest : public ::testing::Test {
 protected:
  TranslateRankerTest() {}

  void SetUp() override {
    locale_ = TranslateDownloadManager::GetInstance()->application_locale();
    TranslateDownloadManager::GetInstance()->set_application_locale("zh-CN");

    // Setup a 50/50 accepted/denied count for "english" when initialize the
    // prefs and translate prefs.
    base::DictionaryValue lang_count;
    lang_count.SetInteger("en", 50);
    prefs_.reset(new sync_preferences::TestingPrefServiceSyncable());
    TranslatePrefs::RegisterProfilePrefs(prefs_->registry());
    prefs_->Set(TranslatePrefs::kPrefTranslateAcceptedCount, lang_count);
    prefs_->Set(TranslatePrefs::kPrefTranslateDeniedCount, lang_count);
    translate_prefs_.reset(new TranslatePrefs(
        prefs_.get(), "intl.accept_languages", kPreferredLanguagePrefs));
    translate_prefs_->SetCountry("us");
  }

  void TearDown() override {
    TranslateDownloadManager::GetInstance()->set_application_locale(locale_);
  }

  void InitFeatures(const std::initializer_list<base::Feature>& enabled,
                    const std::initializer_list<base::Feature>& disabled) {
    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  static std::unique_ptr<TranslateRanker> GetRankerForTest(float bias) {
    chrome_intelligence::TranslateRankerModel model;
    chrome_intelligence::TranslateRankerModel::LogisticRegressionModel*
        details = model.mutable_logistic_regression_model();
    details->set_bias(bias);
    details->set_accept_ratio_weight(0.02f);
    details->set_decline_ratio_weight(0.03f);
    details->set_accept_count_weight(0.13f);
    details->set_decline_count_weight(-0.14f);

    auto& src_language_weight = *details->mutable_source_language_weight();
    src_language_weight["en"] = 0.04f;
    src_language_weight["fr"] = 0.05f;
    src_language_weight["zh"] = 0.06f;

    auto& dest_language_weight = *details->mutable_dest_language_weight();
    dest_language_weight["UNKNOWN"] = 0.00f;

    auto& country_weight = *details->mutable_country_weight();
    country_weight["us"] = 0.07f;
    country_weight["ca"] = 0.08f;
    country_weight["cn"] = 0.09f;

    auto& locale_weight = *details->mutable_locale_weight();
    locale_weight["en-us"] = 0.10f;
    locale_weight["en-ca"] = 0.11f;
    locale_weight["zh-cn"] = 0.12f;  // Normalized to lowercase.

    return TranslateRanker::CreateForTesting(model.SerializeAsString());
  }

  static double Sigmoid(double x) { return 1.0 / (1.0 + exp(-x)); }

  static metrics::TranslateEventProto CreateTranslateEvent(
      const std::string& src_lang,
      const std::string& dst_lang,
      int accept_count,
      int decline_count,
      int ignore_count) {
    metrics::TranslateEventProto translate_event;
    translate_event.set_source_language(src_lang);
    translate_event.set_target_language(dst_lang);
    translate_event.set_accept_count(accept_count);
    translate_event.set_decline_count(decline_count);
    translate_event.set_ignore_count(ignore_count);
    return translate_event;
  }

  static const char* const kPreferredLanguagePrefs;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_;

 private:
  // Manages the enabling/disabling of features within the scope of a test.
  base::test::ScopedFeatureList scoped_feature_list_;

  // Cache and reset the application locale for each test.
  std::string locale_;

  DISALLOW_COPY_AND_ASSIGN(TranslateRankerTest);
};

const char* const TranslateRankerTest::kPreferredLanguagePrefs =
#if defined(OS_CHROMEOS)
    "settings.language.preferred_languages";
#else
    nullptr;
#endif

TEST_F(TranslateRankerTest, DisabledByDefault) {
  InitFeatures({}, {});
  EXPECT_FALSE(TranslateRanker::IsEnabled());
}

TEST_F(TranslateRankerTest, Disabled) {
  InitFeatures({}, {kTranslateRankerQuery, kTranslateRankerEnforcement});
  EXPECT_FALSE(TranslateRanker::IsEnabled());
}

TEST_F(TranslateRankerTest, EnableQuery) {
  InitFeatures({kTranslateRankerQuery}, {kTranslateRankerEnforcement});
  EXPECT_TRUE(TranslateRanker::IsEnabled());
}

TEST_F(TranslateRankerTest, EnableEnforcement) {
  InitFeatures({kTranslateRankerEnforcement}, {kTranslateRankerQuery});
  EXPECT_TRUE(TranslateRanker::IsEnabled());
}

TEST_F(TranslateRankerTest, EnableQueryAndEnforcement) {
  InitFeatures({kTranslateRankerQuery, kTranslateRankerEnforcement}, {});
  EXPECT_TRUE(TranslateRanker::IsEnabled());
  EXPECT_FALSE(TranslateRanker::IsLoggingEnabled());
}

TEST_F(TranslateRankerTest, EnableLogging) {
  InitFeatures({kTranslateRankerLogging}, {});
  EXPECT_FALSE(TranslateRanker::IsEnabled());
  EXPECT_TRUE(TranslateRanker::IsLoggingEnabled());
}


TEST_F(TranslateRankerTest, CalculateScore) {
  InitFeatures({kTranslateRankerQuery, kTranslateRankerEnforcement}, {});
  std::unique_ptr<translate::TranslateRanker> ranker = GetRankerForTest(0.01f);
  // Calculate the score using: a 50:50 accept/decline ratio; the one-hot
  // values for the src lang, dest lang, locale and counry; and, the bias.
  double expected = Sigmoid(0.5 * 0.02f +    // accept ratio * weight
                            0.5 * 0.03f +    // decline ratio * weight
                            0.0 * 0.00f +    // ignore ratio * (default) weight
                            50.0 * 0.13f +   // accept count * weight
                            50.0 * -0.14f +  // decline count * weight
                            0.0 * 0.00f +    // ignore count * (default) weight
                            1.0 * 0.04f +  // one-hot src-language "en" * weight
                            1.0 * 0.00f +  // one-hot dst-language "fr" * weight
                            1.0 * 0.07f +  // one-hot country * weight
                            1.0 * 0.12f +  // one-hot locale * weight
                            0.01f);        // bias

  EXPECT_NEAR(expected,
              ranker->CalculateScore(50, 50, 0, "en", "fr", "zh-CN", "us"),
              0.000001);
}

TEST_F(TranslateRankerTest, ShouldOfferTranslation) {
  InitFeatures({kTranslateRankerQuery, kTranslateRankerEnforcement}, {});
  // With a bias of -0.5 en->fr is not over the threshold.
  EXPECT_FALSE(GetRankerForTest(-0.5f)->ShouldOfferTranslation(
      *translate_prefs_, "en", "fr"));
  // With a bias of 0.25 en-fr is over the threshold.
  EXPECT_TRUE(GetRankerForTest(0.25f)->ShouldOfferTranslation(*translate_prefs_,
                                                              "en", "fr"));
}

TEST_F(TranslateRankerTest, RecordAndFlushEvents) {
  InitFeatures({kTranslateRankerLogging}, {});
  std::unique_ptr<translate::TranslateRanker> ranker = GetRankerForTest(0.0f);
  std::vector<metrics::TranslateEventProto> flushed_events;

  // Check that flushing an empty cache will return an empty vector.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  auto event_1 = CreateTranslateEvent("fr", "en", 1, 0, 3);
  auto event_2 = CreateTranslateEvent("jp", "en", 2, 0, 3);
  auto event_3 = CreateTranslateEvent("es", "de", 4, 5, 6);
  ranker->RecordTranslateEvent(event_1);
  ranker->RecordTranslateEvent(event_2);
  ranker->RecordTranslateEvent(event_3);

  // Capture the data and verify that it is as expected.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(3U, flushed_events.size());
  ASSERT_EQ("fr", flushed_events[0].source_language());
  ASSERT_EQ("jp", flushed_events[1].source_language());
  ASSERT_EQ("es", flushed_events[2].source_language());

  // Check that the cache has been cleared.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());
}

TEST_F(TranslateRankerTest, LoggingDisabled) {
  InitFeatures({}, {kTranslateRankerLogging});
  std::unique_ptr<translate::TranslateRanker> ranker = GetRankerForTest(0.0f);
  std::vector<metrics::TranslateEventProto> flushed_events;

  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  auto event_1 = CreateTranslateEvent("fr", "en", 1, 0, 3);
  auto event_2 = CreateTranslateEvent("jp", "en", 2, 0, 3);
  auto event_3 = CreateTranslateEvent("es", "de", 4, 5, 6);
  ranker->RecordTranslateEvent(event_1);
  ranker->RecordTranslateEvent(event_2);
  ranker->RecordTranslateEvent(event_3);

  // Logging is disabled, so no events should be cached.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());
}

}  // namespace translate
