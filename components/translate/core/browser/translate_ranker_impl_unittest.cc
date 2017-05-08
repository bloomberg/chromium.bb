// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ranker_impl.h"

#include <initializer_list>
#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/metrics/proto/translate_event.pb.h"
#include "components/metrics/proto/ukm/source.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/proto/ranker_model.pb.h"
#include "components/translate/core/browser/proto/translate_ranker_model.pb.h"
#include "components/translate/core/browser/ranker_model.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/ukm/test_ukm_service.h"
#include "components/ukm/ukm_source.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using translate::kTranslateRankerEnforcement;
using translate::kTranslateRankerLogging;
using translate::kTranslateRankerQuery;
using translate::kTranslateRankerDecisionOverride;
using translate::TranslateDownloadManager;
using translate::TranslateRankerFeatures;
using translate::TranslatePrefs;
using translate::TranslateRankerImpl;

constexpr uint32_t kModelVersion = 1234;

class TranslateRankerImplTest : public ::testing::Test {
 protected:
  TranslateRankerImplTest();

  void SetUp() override;

  void TearDown() override;

  // Initializes the explicitly |enabled| and |disabled| features for this test.
  void InitFeatures(const std::initializer_list<base::Feature>& enabled,
                    const std::initializer_list<base::Feature>& disabled);

  // Returns a TranslateRankerImpl object with |bias| for testing.
  std::unique_ptr<TranslateRankerImpl> GetRankerForTest(float bias);

  // Implements the same sigmoid function used by TranslateRankerImpl.
  static double Sigmoid(double x);

  // Returns a translate event initialized with the given parameters.
  static metrics::TranslateEventProto CreateTranslateEvent(
      const std::string& src_lang,
      const std::string& dst_lang,
      int accept_count,
      int decline_count,
      int ignore_count);

  // The platform-specific name of the preferred language pref.
  static const char* const kPreferredLanguagePref;

  // Prefs.
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_;

  ukm::TestUkmService* GetTestUkmService() {
    return ukm_service_test_harness_.test_ukm_service();
  }
  metrics::TranslateEventProto tep1_ =
      CreateTranslateEvent("fr", "en", 1, 0, 3);
  metrics::TranslateEventProto tep2_ =
      CreateTranslateEvent("jp", "en", 2, 0, 3);
  metrics::TranslateEventProto tep3_ =
      CreateTranslateEvent("es", "de", 4, 5, 6);

 private:
  ukm::UkmServiceTestingHarness ukm_service_test_harness_;

  // Override the default URL fetcher to return custom responses for tests.
  net::TestURLFetcherFactory url_fetcher_factory_;

  // Used to initialize the translate download manager.
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;

  // Sets up the task scheduling/task-runner environment for each test.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Manages the enabling/disabling of features within the scope of a test.
  base::test::ScopedFeatureList scoped_feature_list_;

  // Cache and reset the application locale for each test.
  std::string locale_;

  DISALLOW_COPY_AND_ASSIGN(TranslateRankerImplTest);
};

const char* const TranslateRankerImplTest::kPreferredLanguagePref =
#if defined(OS_CHROMEOS)
    "settings.language.preferred_languages";
#else
    nullptr;
#endif

TranslateRankerImplTest::TranslateRankerImplTest() {}

void TranslateRankerImplTest::SetUp() {
  // Setup the translate download manager.
  locale_ = TranslateDownloadManager::GetInstance()->application_locale();
  request_context_ =
      new net::TestURLRequestContextGetter(base::ThreadTaskRunnerHandle::Get());
  TranslateDownloadManager::GetInstance()->set_application_locale("zh-CN");
  TranslateDownloadManager::GetInstance()->set_request_context(
      request_context_.get());

  // Setup a 50/50 accepted/denied count for "english" when initialize the
  // prefs and translate prefs.
  base::DictionaryValue lang_count;
  lang_count.SetInteger("en", 50);
  prefs_.reset(new sync_preferences::TestingPrefServiceSyncable());
  TranslatePrefs::RegisterProfilePrefs(prefs_->registry());
  prefs_->Set(TranslatePrefs::kPrefTranslateAcceptedCount, lang_count);
  prefs_->Set(TranslatePrefs::kPrefTranslateDeniedCount, lang_count);
  translate_prefs_.reset(new TranslatePrefs(
      prefs_.get(), "intl.accept_languages", kPreferredLanguagePref));
  translate_prefs_->SetCountry("de");
}

void TranslateRankerImplTest::TearDown() {
  base::RunLoop().RunUntilIdle();
  TranslateDownloadManager::GetInstance()->set_application_locale(locale_);
  TranslateDownloadManager::GetInstance()->set_request_context(nullptr);
}

void TranslateRankerImplTest::InitFeatures(
    const std::initializer_list<base::Feature>& enabled,
    const std::initializer_list<base::Feature>& disabled) {
  scoped_feature_list_.InitWithFeatures(enabled, disabled);
}

std::unique_ptr<TranslateRankerImpl> TranslateRankerImplTest::GetRankerForTest(
    float bias) {
  auto model = base::MakeUnique<chrome_intelligence::RankerModel>();
  model->mutable_proto()->mutable_translate()->set_version(kModelVersion);
  auto* details = model->mutable_proto()
                      ->mutable_translate()
                      ->mutable_logistic_regression_model();
  details->set_bias(bias);
  details->set_accept_ratio_weight(0.02f);
  details->set_decline_ratio_weight(0.03f);
  details->set_accept_count_weight(0.13f);
  details->set_decline_count_weight(-0.14f);

  auto& src_language_weight = *details->mutable_source_language_weight();
  src_language_weight["en"] = 0.04f;
  src_language_weight["fr"] = 0.05f;
  src_language_weight["zh"] = 0.06f;

  auto& target_language_weight = *details->mutable_target_language_weight();
  target_language_weight["UNKNOWN"] = 0.00f;

  auto& country_weight = *details->mutable_country_weight();
  country_weight["de"] = 0.07f;
  country_weight["ca"] = 0.08f;
  country_weight["cn"] = 0.09f;

  auto& locale_weight = *details->mutable_locale_weight();
  locale_weight["en-us"] = 0.10f;
  locale_weight["en-ca"] = 0.11f;
  locale_weight["zh-cn"] = 0.12f;  // Normalized to lowercase.

  auto impl = base::MakeUnique<TranslateRankerImpl>(base::FilePath(), GURL(),
                                                    GetTestUkmService());
  impl->OnModelAvailable(std::move(model));
  base::RunLoop().RunUntilIdle();
  return impl;
}

// static
double TranslateRankerImplTest::Sigmoid(double x) {
  return 1.0 / (1.0 + exp(-x));
}

// static
metrics::TranslateEventProto TranslateRankerImplTest::CreateTranslateEvent(
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

}  // namespace

TEST_F(TranslateRankerImplTest, GetVersion) {
  InitFeatures({kTranslateRankerQuery}, {});
  auto ranker = GetRankerForTest(0.01f);
  EXPECT_TRUE(ranker->CheckModelLoaderForTesting());
  EXPECT_EQ(kModelVersion, ranker->GetModelVersion());
}

TEST_F(TranslateRankerImplTest, ModelLoaderQueryNotEnabled) {
  // If Query is not enabled, the ranker should not try to load the model.
  InitFeatures({}, {kTranslateRankerQuery});
  auto ranker = GetRankerForTest(0.01f);
  EXPECT_FALSE(ranker->CheckModelLoaderForTesting());
}

TEST_F(TranslateRankerImplTest, CalculateScore) {
  InitFeatures({kTranslateRankerEnforcement}, {});
  auto ranker = GetRankerForTest(0.01f);
  // Calculate the score using: a 50:50 accept/decline ratio; the one-hot
  // values for the src lang, dest lang, locale and counry; and, the bias.
  double expected = Sigmoid(50.0 * 0.13f +   // accept count * weight
                            50.0 * -0.14f +  // decline count * weight
                            0.0 * 0.00f +    // ignore count * (default) weight
                            0.5 * 0.02f +    // accept ratio * weight
                            0.5 * 0.03f +    // decline ratio * weight
                            0.0 * 0.00f +    // ignore ratio * (default) weight
                            1.0 * 0.04f +  // one-hot src-language "en" * weight
                            1.0 * 0.00f +  // one-hot dst-language "fr" * weight
                            1.0 * 0.07f +  // one-hot country "de" * weight
                            1.0 * 0.12f +  // one-hot locale "zh-CN" * weight
                            0.01f);        // bias
  TranslateRankerFeatures features(50, 50, 0, "en", "fr", "de", "zh-CN");

  EXPECT_NEAR(expected, ranker->CalculateScore(features), 0.000001);
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_AllEnabled) {
  InitFeatures({kTranslateRankerQuery, kTranslateRankerEnforcement,
                kTranslateRankerDecisionOverride},
               {});
  metrics::TranslateEventProto tep;

  // With a bias of -0.5 en->fr is not over the threshold.
  EXPECT_FALSE(GetRankerForTest(-0.5f)->ShouldOfferTranslation(
      *translate_prefs_, "en", "fr", &tep));
  EXPECT_NE(0U, tep.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, tep.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::DONT_SHOW, tep.ranker_response());

  // With a bias of 0.25 en-fr is over the threshold.
  tep.Clear();
  EXPECT_TRUE(GetRankerForTest(0.25f)->ShouldOfferTranslation(
      *translate_prefs_, "en", "fr", &tep));
  EXPECT_EQ(metrics::TranslateEventProto::SHOW, tep.ranker_response());
}
TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_AllDisabled) {
  InitFeatures({}, {kTranslateRankerQuery, kTranslateRankerEnforcement,
                    kTranslateRankerDecisionOverride});
  metrics::TranslateEventProto tep;
  // If query and other flags are turned off, returns true and do not query the
  // ranker.
  EXPECT_TRUE(GetRankerForTest(-0.5f)->ShouldOfferTranslation(
      *translate_prefs_, "en", "fr", &tep));
  EXPECT_NE(0U, tep.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, tep.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::NOT_QUERIED, tep.ranker_response());
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_QueryOnly) {
  InitFeatures({kTranslateRankerQuery},
               {kTranslateRankerEnforcement, kTranslateRankerDecisionOverride});
  metrics::TranslateEventProto tep;
  // If enforcement is turned off, returns true even if the decision
  // is not to show.
  EXPECT_TRUE(GetRankerForTest(-0.5f)->ShouldOfferTranslation(
      *translate_prefs_, "en", "fr", &tep));
  EXPECT_NE(0U, tep.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, tep.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::DONT_SHOW, tep.ranker_response());
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_EnforcementOnly) {
  InitFeatures({kTranslateRankerEnforcement},
               {kTranslateRankerQuery, kTranslateRankerDecisionOverride});
  metrics::TranslateEventProto tep;
  // If either enforcement or decision override are turned on, returns the
  // ranker decision.
  EXPECT_FALSE(GetRankerForTest(-0.5f)->ShouldOfferTranslation(
      *translate_prefs_, "en", "fr", &tep));
  EXPECT_NE(0U, tep.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, tep.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::DONT_SHOW, tep.ranker_response());
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_OverrideOnly) {
  InitFeatures({kTranslateRankerDecisionOverride},
               {kTranslateRankerQuery, kTranslateRankerEnforcement});
  metrics::TranslateEventProto tep;
  // If either enforcement or decision override are turned on, returns the
  // ranker decision.
  EXPECT_FALSE(GetRankerForTest(-0.5f)->ShouldOfferTranslation(
      *translate_prefs_, "en", "fr", &tep));
  EXPECT_NE(0U, tep.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, tep.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::DONT_SHOW, tep.ranker_response());
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_NoModel) {
  auto ranker =
      base::MakeUnique<TranslateRankerImpl>(base::FilePath(), GURL(), nullptr);
  InitFeatures({kTranslateRankerDecisionOverride, kTranslateRankerQuery,
                kTranslateRankerEnforcement},
               {});
  metrics::TranslateEventProto tep;
  // If we don't have a model, returns true.
  EXPECT_TRUE(
      ranker->ShouldOfferTranslation(*translate_prefs_, "en", "fr", &tep));
  EXPECT_NE(0U, tep.ranker_request_timestamp_sec());
  EXPECT_EQ(0U, tep.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::NOT_QUERIED, tep.ranker_response());
}

TEST_F(TranslateRankerImplTest, RecordAndFlushEvents) {
  InitFeatures({kTranslateRankerLogging}, {});
  std::unique_ptr<translate::TranslateRanker> ranker = GetRankerForTest(0.0f);
  std::vector<metrics::TranslateEventProto> flushed_events;

  GURL url0("https://www.google.com");
  GURL url1("https://www.gmail.com");

  // Check that flushing an empty cache will return an empty vector.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  ranker->RecordTranslateEvent(0, url0, &tep1_);
  ranker->RecordTranslateEvent(1, GURL(), &tep2_);
  ranker->RecordTranslateEvent(2, url1, &tep3_);

  // Capture the data and verify that it is as expected.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(3U, flushed_events.size());
  ASSERT_EQ(tep1_.source_language(), flushed_events[0].source_language());
  ASSERT_EQ(0, flushed_events[0].event_type());
  ASSERT_EQ(tep2_.source_language(), flushed_events[1].source_language());
  ASSERT_EQ(1, flushed_events[1].event_type());
  ASSERT_EQ(tep3_.source_language(), flushed_events[2].source_language());
  ASSERT_EQ(2, flushed_events[2].event_type());

  // Check that the cache has been cleared.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  ASSERT_EQ(2U, GetTestUkmService()->sources_count());
  EXPECT_EQ(
      url0.spec(),
      GetTestUkmService()->GetSourceForUrl(url0.spec().c_str())->url().spec());
  EXPECT_EQ(
      url1.spec(),
      GetTestUkmService()->GetSourceForUrl(url1.spec().c_str())->url().spec());
}

TEST_F(TranslateRankerImplTest, LoggingDisabled) {
  InitFeatures({}, {kTranslateRankerLogging});
  std::unique_ptr<translate::TranslateRanker> ranker = GetRankerForTest(0.0f);
  std::vector<metrics::TranslateEventProto> flushed_events;

  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  ranker->RecordTranslateEvent(0, GURL(), &tep1_);
  ranker->RecordTranslateEvent(1, GURL(), &tep2_);
  ranker->RecordTranslateEvent(2, GURL(), &tep3_);

  // Logging is disabled, so no events should be cached.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());
  EXPECT_EQ(0ul, GetTestUkmService()->sources_count());
}

TEST_F(TranslateRankerImplTest, LoggingDisabledViaOverride) {
  InitFeatures({kTranslateRankerLogging}, {});
  std::unique_ptr<translate::TranslateRankerImpl> ranker =
      GetRankerForTest(0.0f);
  std::vector<metrics::TranslateEventProto> flushed_events;

  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  ranker->RecordTranslateEvent(0, GURL(), &tep1_);
  ranker->RecordTranslateEvent(1, GURL(), &tep2_);
  ranker->RecordTranslateEvent(2, GURL(), &tep3_);

  // Logging is disabled, so no events should be cached.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(3U, flushed_events.size());

  // Override the feature setting to disable logging.
  ranker->EnableLogging(false);

  ranker->RecordTranslateEvent(0, GURL(), &tep1_);
  ranker->RecordTranslateEvent(1, GURL(), &tep2_);
  ranker->RecordTranslateEvent(2, GURL(), &tep3_);

  // Logging is disabled, so no events should be cached.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());
}

TEST_F(TranslateRankerImplTest, ShouldOverrideDecision_OverrideDisabled) {
  InitFeatures({}, {kTranslateRankerDecisionOverride});
  std::unique_ptr<translate::TranslateRankerImpl> ranker =
      GetRankerForTest(0.0f);
  const int kEventType = 12;
  metrics::TranslateEventProto tep = CreateTranslateEvent("fr", "en", 1, 0, 3);

  EXPECT_FALSE(ranker->ShouldOverrideDecision(kEventType, GURL(), &tep));

  std::vector<metrics::TranslateEventProto> flushed_events;
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(1U, flushed_events.size());
  ASSERT_EQ(tep.source_language(), flushed_events[0].source_language());
  ASSERT_EQ(kEventType, flushed_events[0].event_type());
}

TEST_F(TranslateRankerImplTest, ShouldOverrideDecision_OverrideEnabled) {
  InitFeatures({kTranslateRankerDecisionOverride}, {});
  std::unique_ptr<translate::TranslateRankerImpl> ranker =
      GetRankerForTest(0.0f);
  metrics::TranslateEventProto tep = CreateTranslateEvent("fr", "en", 1, 0, 3);

  EXPECT_TRUE(ranker->ShouldOverrideDecision(1, GURL(), &tep));
  EXPECT_TRUE(ranker->ShouldOverrideDecision(2, GURL(), &tep));

  std::vector<metrics::TranslateEventProto> flushed_events;
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());
  ASSERT_EQ(2, tep.decision_overrides_size());
  ASSERT_EQ(1, tep.decision_overrides(0));
  ASSERT_EQ(2, tep.decision_overrides(1));
  ASSERT_EQ(0, tep.event_type());
}
