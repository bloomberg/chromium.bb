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
#include "components/machine_intelligence/proto/ranker_model.pb.h"
#include "components/machine_intelligence/proto/translate_ranker_model.pb.h"
#include "components/machine_intelligence/ranker_model.h"
#include "components/metrics/proto/translate_event.pb.h"
#include "components/metrics/proto/ukm/source.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using translate::kTranslateRankerEnforcement;
using translate::kTranslateRankerQuery;
using translate::kTranslateRankerDecisionOverride;
using translate::TranslateRankerFeatures;
using translate::TranslateRankerImpl;

constexpr uint32_t kModelVersion = 1234;

class TranslateRankerImplTest : public ::testing::Test {
 protected:
  TranslateRankerImplTest();

  // Initializes the explicitly |enabled| and |disabled| features for this test.
  void InitFeatures(const std::initializer_list<base::Feature>& enabled,
                    const std::initializer_list<base::Feature>& disabled);

  // Returns a TranslateRankerImpl object with |threshold| for testing. The
  // returned ranker is configured with an empty cache path and URL and will not
  // invoke the model loader.
  std::unique_ptr<TranslateRankerImpl> GetRankerForTest(float threshold);

  // Implements the same sigmoid function used by TranslateRankerImpl.
  static double Sigmoid(double x);

  // Returns a translate event initialized with the given parameters.
  static metrics::TranslateEventProto CreateTranslateEvent(
      const std::string& src_lang,
      const std::string& dst_lang,
      const std::string& country,
      int accept_count,
      int decline_count,
      int ignore_count);

  // Returns a translate event initialized with the given parameters.
  static metrics::TranslateEventProto CreateDefaultTranslateEvent();

  ukm::TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }
  metrics::TranslateEventProto translate_event1_ =
      CreateTranslateEvent("fr", "en", "de", 1, 0, 3);
  metrics::TranslateEventProto translate_event2_ =
      CreateTranslateEvent("jp", "en", "de", 2, 0, 3);
  metrics::TranslateEventProto translate_event3_ =
      CreateTranslateEvent("es", "de", "de", 4, 5, 6);

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  // Sets up the task scheduling/task-runner environment for each test.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Manages the enabling/disabling of features within the scope of a test.
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(TranslateRankerImplTest);
};

TranslateRankerImplTest::TranslateRankerImplTest() {}

void TranslateRankerImplTest::InitFeatures(
    const std::initializer_list<base::Feature>& enabled,
    const std::initializer_list<base::Feature>& disabled) {
  scoped_feature_list_.InitWithFeatures(enabled, disabled);
}

std::unique_ptr<TranslateRankerImpl> TranslateRankerImplTest::GetRankerForTest(
    float threshold) {
  auto model = base::MakeUnique<machine_intelligence::RankerModel>();
  model->mutable_proto()->mutable_translate()->set_version(kModelVersion);
  auto* details = model->mutable_proto()
                      ->mutable_translate()
                      ->mutable_logistic_regression_model();
  if (threshold > 0.0) {
    details->set_threshold(threshold);
  }
  details->set_bias(0.5f);
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

  auto impl = base::MakeUnique<TranslateRankerImpl>(base::FilePath(), GURL(),
                                                    GetTestUkmRecorder());
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
    const std::string& country,
    int accept_count,
    int decline_count,
    int ignore_count) {
  metrics::TranslateEventProto translate_event;
  translate_event.set_source_language(src_lang);
  translate_event.set_target_language(dst_lang);
  translate_event.set_country(country);
  translate_event.set_accept_count(accept_count);
  translate_event.set_decline_count(decline_count);
  translate_event.set_ignore_count(ignore_count);
  return translate_event;
}

// static
metrics::TranslateEventProto
TranslateRankerImplTest::CreateDefaultTranslateEvent() {
  return TranslateRankerImplTest::CreateTranslateEvent("en", "fr", "de", 50, 50,
                                                       0);
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

TEST_F(TranslateRankerImplTest, GetModelDecision) {
  InitFeatures({kTranslateRankerEnforcement}, {});
  // Calculate the score using: a 50:50 accept/decline ratio; the one-hot
  // values for the src lang, dest lang, and country; and, the bias.
  double expected_score =
      Sigmoid(50.0 * 0.13f +   // accept count * weight
              50.0 * -0.14f +  // decline count * weight
              0.0 * 0.00f +    // ignore count * (default) weight
              0.5 * 0.02f +    // accept ratio * weight
              0.5 * 0.03f +    // decline ratio * weight
              0.0 * 0.00f +    // ignore ratio * (default) weight
              1.0 * 0.04f +    // one-hot src-language "en" * weight
              1.0 * 0.00f +    // one-hot dst-language "fr" * weight
              1.0 * 0.07f +    // one-hot country "de" * weight
              0.5f);           // bias
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();

  const float epsilon = 0.001f;
  auto ranker = GetRankerForTest(expected_score + epsilon);
  EXPECT_FALSE(ranker->GetModelDecision(translate_event));

  ranker = GetRankerForTest(expected_score - epsilon);
  EXPECT_TRUE(ranker->GetModelDecision(translate_event));

  ranker = GetRankerForTest(0.0);
  EXPECT_EQ(expected_score >= 0.5, ranker->GetModelDecision(translate_event));
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_AllEnabled) {
  InitFeatures({kTranslateRankerQuery, kTranslateRankerEnforcement,
                kTranslateRankerDecisionOverride},
               {});
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();

  // With a threshold of 0.99, en->fr is not over the threshold.
  EXPECT_FALSE(
      GetRankerForTest(0.99f)->ShouldOfferTranslation(&translate_event));
  EXPECT_NE(0U, translate_event.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, translate_event.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::DONT_SHOW,
            translate_event.ranker_response());

  // With a threshold of 0.01, en-fr is over the threshold.
  translate_event.Clear();
  EXPECT_TRUE(
      GetRankerForTest(0.01f)->ShouldOfferTranslation(&translate_event));
  EXPECT_EQ(metrics::TranslateEventProto::SHOW,
            translate_event.ranker_response());
}
TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_AllDisabled) {
  InitFeatures({}, {kTranslateRankerQuery, kTranslateRankerEnforcement,
                    kTranslateRankerDecisionOverride});
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();
  // If query and other flags are turned off, returns true and do not query the
  // ranker.
  EXPECT_TRUE(GetRankerForTest(0.5f)->ShouldOfferTranslation(&translate_event));
  EXPECT_NE(0U, translate_event.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, translate_event.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::NOT_QUERIED,
            translate_event.ranker_response());
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_QueryOnlyDontShow) {
  InitFeatures({kTranslateRankerQuery},
               {kTranslateRankerEnforcement, kTranslateRankerDecisionOverride});
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();
  // If enforcement is turned off, returns true even if the decision
  // is not to show.
  EXPECT_TRUE(
      GetRankerForTest(0.99f)->ShouldOfferTranslation(&translate_event));
  EXPECT_NE(0U, translate_event.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, translate_event.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::DONT_SHOW,
            translate_event.ranker_response());
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_QueryOnlyShow) {
  InitFeatures({kTranslateRankerQuery},
               {kTranslateRankerEnforcement, kTranslateRankerDecisionOverride});
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();
  EXPECT_TRUE(
      GetRankerForTest(0.01f)->ShouldOfferTranslation(&translate_event));
  EXPECT_NE(0U, translate_event.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, translate_event.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::SHOW,
            translate_event.ranker_response());
}

TEST_F(TranslateRankerImplTest,
       ShouldOfferTranslation_EnforcementOnlyDontShow) {
  InitFeatures({kTranslateRankerEnforcement},
               {kTranslateRankerQuery, kTranslateRankerDecisionOverride});
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();
  // If enforcement is turned on, returns the ranker decision.
  EXPECT_FALSE(
      GetRankerForTest(0.99f)->ShouldOfferTranslation(&translate_event));
  EXPECT_NE(0U, translate_event.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, translate_event.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::DONT_SHOW,
            translate_event.ranker_response());
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_EnforcementOnlyShow) {
  InitFeatures({kTranslateRankerEnforcement},
               {kTranslateRankerQuery, kTranslateRankerDecisionOverride});
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();
  // If enforcement is turned on, returns the ranker decision.
  EXPECT_TRUE(
      GetRankerForTest(0.01f)->ShouldOfferTranslation(&translate_event));
  EXPECT_NE(0U, translate_event.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, translate_event.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::SHOW,
            translate_event.ranker_response());
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_OverrideAndEnforcement) {
  InitFeatures({kTranslateRankerEnforcement, kTranslateRankerDecisionOverride},
               {kTranslateRankerQuery});
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();
  // DecisionOverride will not interact with Query or Enforcement.
  EXPECT_FALSE(
      GetRankerForTest(0.99f)->ShouldOfferTranslation(&translate_event));
  EXPECT_NE(0U, translate_event.ranker_request_timestamp_sec());
  EXPECT_EQ(kModelVersion, translate_event.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::DONT_SHOW,
            translate_event.ranker_response());
}

TEST_F(TranslateRankerImplTest, ShouldOfferTranslation_NoModel) {
  auto ranker =
      base::MakeUnique<TranslateRankerImpl>(base::FilePath(), GURL(), nullptr);
  InitFeatures({kTranslateRankerDecisionOverride, kTranslateRankerQuery,
                kTranslateRankerEnforcement},
               {});
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();
  // If we don't have a model, returns true.
  EXPECT_TRUE(ranker->ShouldOfferTranslation(&translate_event));
  EXPECT_NE(0U, translate_event.ranker_request_timestamp_sec());
  EXPECT_EQ(0U, translate_event.ranker_version());
  EXPECT_EQ(metrics::TranslateEventProto::NOT_QUERIED,
            translate_event.ranker_response());
}

TEST_F(TranslateRankerImplTest, RecordAndFlushEvents) {
  std::unique_ptr<translate::TranslateRanker> ranker = GetRankerForTest(0.0f);
  ranker->EnableLogging(true);
  std::vector<metrics::TranslateEventProto> flushed_events;

  GURL url0("https://www.google.com");
  GURL url1("https://www.gmail.com");

  // Check that flushing an empty cache will return an empty vector.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  ranker->RecordTranslateEvent(0, url0, &translate_event1_);
  ranker->RecordTranslateEvent(1, GURL(), &translate_event2_);
  ranker->RecordTranslateEvent(2, url1, &translate_event3_);

  // Capture the data and verify that it is as expected.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(3U, flushed_events.size());
  ASSERT_EQ(translate_event1_.source_language(),
            flushed_events[0].source_language());
  ASSERT_EQ(0, flushed_events[0].event_type());
  ASSERT_EQ(translate_event2_.source_language(),
            flushed_events[1].source_language());
  ASSERT_EQ(1, flushed_events[1].event_type());
  ASSERT_EQ(translate_event3_.source_language(),
            flushed_events[2].source_language());
  ASSERT_EQ(2, flushed_events[2].event_type());

  // Check that the cache has been cleared.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  ASSERT_EQ(2U, GetTestUkmRecorder()->sources_count());
  EXPECT_EQ(
      url0.spec(),
      GetTestUkmRecorder()->GetSourceForUrl(url0.spec().c_str())->url().spec());
  EXPECT_EQ(
      url1.spec(),
      GetTestUkmRecorder()->GetSourceForUrl(url1.spec().c_str())->url().spec());
}

TEST_F(TranslateRankerImplTest, EnableLogging) {
  std::unique_ptr<translate::TranslateRankerImpl> ranker =
      GetRankerForTest(0.0f);
  std::vector<metrics::TranslateEventProto> flushed_events;

  // Logging is disabled by default. No events will be cached.
  ranker->RecordTranslateEvent(0, GURL(), &translate_event1_);
  ranker->RecordTranslateEvent(1, GURL(), &translate_event2_);

  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  // Once we enable logging, events will be cached.
  ranker->EnableLogging(true);
  ranker->RecordTranslateEvent(0, GURL(), &translate_event1_);
  ranker->RecordTranslateEvent(1, GURL(), &translate_event2_);

  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(2U, flushed_events.size());
  flushed_events.clear();

  // Turning logging back off, caching is disabled once again.
  ranker->EnableLogging(false);
  ranker->RecordTranslateEvent(0, GURL(), &translate_event1_);
  ranker->RecordTranslateEvent(1, GURL(), &translate_event2_);

  // Logging is disabled, so no events should be cached.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());
}

TEST_F(TranslateRankerImplTest, EnableLoggingClearsCache) {
  std::unique_ptr<translate::TranslateRankerImpl> ranker =
      GetRankerForTest(0.0f);
  std::vector<metrics::TranslateEventProto> flushed_events;
  // Logging is disabled by default. No events will be cached.
  ranker->RecordTranslateEvent(0, GURL(), &translate_event1_);
  // Making sure that cache is still empty once logging is turned on.
  ranker->EnableLogging(true);
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  // These events will be cached.
  ranker->RecordTranslateEvent(0, GURL(), &translate_event1_);
  ranker->RecordTranslateEvent(1, GURL(), &translate_event2_);
  // Cache will not be cleared if the logging state does not change.
  ranker->EnableLogging(true);
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(2U, flushed_events.size());
  flushed_events.clear();
  // Cache is now empty after being flushed.
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  // Filling cache again.
  ranker->EnableLogging(true);
  ranker->RecordTranslateEvent(0, GURL(), &translate_event1_);
  ranker->RecordTranslateEvent(1, GURL(), &translate_event2_);
  // Switching logging off will clear the cache.
  ranker->EnableLogging(false);
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());

  // Filling cache again.
  ranker->EnableLogging(true);
  ranker->RecordTranslateEvent(0, GURL(), &translate_event1_);
  ranker->RecordTranslateEvent(1, GURL(), &translate_event2_);
  // Switching logging off and on again will clear the cache.
  ranker->EnableLogging(false);
  ranker->EnableLogging(true);
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());
}

TEST_F(TranslateRankerImplTest, ShouldOverrideDecision_OverrideDisabled) {
  InitFeatures({}, {kTranslateRankerDecisionOverride});
  std::unique_ptr<translate::TranslateRankerImpl> ranker =
      GetRankerForTest(0.0f);
  ranker->EnableLogging(true);
  const int kEventType = 12;
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();

  EXPECT_FALSE(
      ranker->ShouldOverrideDecision(kEventType, GURL(), &translate_event));

  std::vector<metrics::TranslateEventProto> flushed_events;
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(1U, flushed_events.size());
  ASSERT_EQ(translate_event.source_language(),
            flushed_events[0].source_language());
  ASSERT_EQ(kEventType, flushed_events[0].event_type());
}

TEST_F(TranslateRankerImplTest, ShouldOverrideDecision_OverrideEnabled) {
  InitFeatures({kTranslateRankerDecisionOverride},
               {kTranslateRankerQuery, kTranslateRankerEnforcement});
  std::unique_ptr<translate::TranslateRankerImpl> ranker =
      GetRankerForTest(0.0f);
  ranker->EnableLogging(true);
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();
  // DecisionOverride is decoupled from querying and enforcement. Enabling
  // only DecisionOverride will not query the Ranker. Ranker returns its default
  // response.
  EXPECT_TRUE(
      GetRankerForTest(0.99f)->ShouldOfferTranslation(&translate_event));
  EXPECT_TRUE(ranker->ShouldOverrideDecision(1, GURL(), &translate_event));
  EXPECT_TRUE(ranker->ShouldOverrideDecision(2, GURL(), &translate_event));

  std::vector<metrics::TranslateEventProto> flushed_events;
  ranker->FlushTranslateEvents(&flushed_events);
  EXPECT_EQ(0U, flushed_events.size());
  ASSERT_EQ(2, translate_event.decision_overrides_size());
  ASSERT_EQ(1, translate_event.decision_overrides(0));
  ASSERT_EQ(2, translate_event.decision_overrides(1));
  ASSERT_EQ(0, translate_event.event_type());
  EXPECT_EQ(metrics::TranslateEventProto::NOT_QUERIED,
            translate_event.ranker_response());
}

TEST_F(TranslateRankerImplTest,
       ShouldOverrideDecision_OverrideAndQueryEnabled) {
  InitFeatures({kTranslateRankerDecisionOverride, kTranslateRankerQuery},
               {kTranslateRankerEnforcement});
  // This test checks that translate events are properly logged when ranker is
  // queried and a decision is overridden.
  std::unique_ptr<translate::TranslateRankerImpl> ranker =
      GetRankerForTest(0.0f);
  ranker->EnableLogging(true);
  metrics::TranslateEventProto translate_event = CreateDefaultTranslateEvent();
  // Ranker's decision is DONT_SHOW, but we are in query mode only, so Ranker
  // does not suppress the UI.
  EXPECT_TRUE(
      GetRankerForTest(0.99f)->ShouldOfferTranslation(&translate_event));

  int kOverrideType = 1;
  EXPECT_TRUE(
      ranker->ShouldOverrideDecision(kOverrideType, GURL(), &translate_event));

  int kEventType = 5;
  ranker->RecordTranslateEvent(kEventType, GURL(), &translate_event);
  std::vector<metrics::TranslateEventProto> flushed_events;
  ranker->FlushTranslateEvents(&flushed_events);

  EXPECT_EQ(1U, flushed_events.size());
  ASSERT_EQ(1, flushed_events[0].decision_overrides_size());
  ASSERT_EQ(kOverrideType, flushed_events[0].decision_overrides(0));
  ASSERT_EQ(kEventType, flushed_events[0].event_type());
  EXPECT_EQ(metrics::TranslateEventProto::DONT_SHOW,
            flushed_events[0].ranker_response());
}
