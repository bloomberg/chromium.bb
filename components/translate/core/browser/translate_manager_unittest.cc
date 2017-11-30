// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/language_model.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/translate_event.pb.h"

using testing::_;
using testing::Return;
using testing::SetArgPointee;
using testing::Pointee;

namespace translate {

namespace {

const char kInitiationStatusName[] = "Translate.InitiationStatus.v2";

// Overrides NetworkChangeNotifier, simulating connection type changes
// for tests.
// TODO(groby): Combine with similar code in ResourceRequestAllowedNotifierTest.
class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : net::NetworkChangeNotifier(),
        connection_type_to_return_(
            net::NetworkChangeNotifier::CONNECTION_UNKNOWN) {}

  // Simulates a change of the connection type to |type|. This will notify any
  // objects that are NetworkChangeNotifiers.
  void SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::ConnectionType type) {
    connection_type_to_return_ = type;
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        connection_type_to_return_);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateOffline() {
    connection_type_to_return_ = net::NetworkChangeNotifier::CONNECTION_NONE;
  }

  void SimulateOnline() {
    connection_type_to_return_ = net::NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }

 private:
  ConnectionType GetCurrentConnectionType() const override {
    return connection_type_to_return_;
  }

  // The currently simulated network connection type. If this is set to
  // CONNECTION_NONE, then NetworkChangeNotifier::IsOffline will return true.
  net::NetworkChangeNotifier::ConnectionType connection_type_to_return_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

// Compares TranslateEventProto on a restricted set of fields.
MATCHER_P(EqualsTranslateEventProto, translate_event, "") {
  const metrics::TranslateEventProto& tep(translate_event);
  return (arg.source_language() == tep.source_language() &&
          arg.target_language() == tep.target_language() &&
          arg.event_type() == tep.event_type());
}

// A language model that just returns its instance variable.
class MockLanguageModel : public language::LanguageModel {
 public:
  explicit MockLanguageModel(const std::vector<LanguageDetails>& in_details)
      : details(in_details) {}

  std::vector<LanguageDetails> GetLanguages() override { return details; }

  std::vector<LanguageDetails> details;
};

}  // namespace

namespace testing {

class TranslateManagerTest : public ::testing::Test {
 protected:
  TranslateManagerTest()
      : translate_prefs_(&prefs_,
                         accept_languages_prefs,
                         preferred_languages_prefs),
        manager_(TranslateDownloadManager::GetInstance()),
        mock_translate_client_(&driver_, &prefs_),
        mock_language_model_({MockLanguageModel::LanguageDetails("en", 1.0)}),
        field_trial_list_(new base::FieldTrialList(nullptr)) {}

  void SetUp() override {
    // Ensure we're not requesting a server-side translate language list.
    TranslateLanguageList::DisableUpdate();
    prefs_.registry()->RegisterStringPref(accept_languages_prefs,
                                          std::string());
#if defined(OS_CHROMEOS)
    prefs_.registry()->RegisterStringPref(preferred_languages_prefs,
                                          std::string());
#endif
    TranslatePrefs::RegisterProfilePrefs(prefs_.registry());
    // TODO(groby): Figure out RegisterProfilePrefs() should register this.
    prefs_.registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    manager_->ResetForTesting();
  }

  void TearDown() override {
    manager_->ResetForTesting();
    variations::testing::ClearAllVariationParams();
  }

  // Utility function to prepare translate_manager_ for testing.
  void PrepareTranslateManager() {
    TranslateManager::SetIgnoreMissingKeyForTesting(true);
    translate_manager_.reset(new translate::TranslateManager(
        &mock_translate_client_, &mock_translate_ranker_,
        &mock_language_model_));
  }

  void SetHasLanguageChanged(bool has_language_changed) {
    translate_manager_->GetLanguageState().LanguageDetermined("de", true);
    translate_manager_->GetLanguageState().DidNavigate(false, true, false);
    translate_manager_->GetLanguageState().LanguageDetermined(
        has_language_changed ? "en" : "de", true);
    EXPECT_EQ(has_language_changed,
              translate_manager_->GetLanguageState().HasLanguageChanged());
  }

  void SetLanguageTooOftenDenied(const std::string& language) {
    if (base::FeatureList::IsEnabled(kTranslateUI2016Q2)) {
      translate_prefs_.ResetDenialState();
      for (int i = 0; i < 4; i++) {
        translate_prefs_.IncrementTranslationDeniedCount(language);
      }
    } else {
      translate_prefs_.UpdateLastDeniedTime(language);
      translate_prefs_.UpdateLastDeniedTime(language);
    }

    EXPECT_TRUE(translate_prefs_.IsTooOftenDenied(language));
    EXPECT_FALSE(translate_prefs_.IsTooOftenDenied("other_language"));
  }

  void InitTranslateEvent(const std::string& src_lang,
                          const std::string& dst_lang) {
    translate_manager_->InitTranslateEvent(src_lang, dst_lang,
                                           translate_prefs_);
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;

  // TODO(groby): request TranslatePrefs from |mock_translate_client_| instead.
  TranslatePrefs translate_prefs_;
  TranslateDownloadManager* manager_;

  TestNetworkChangeNotifier network_notifier_;
  translate::testing::MockTranslateDriver driver_;
  translate::testing::MockTranslateRanker mock_translate_ranker_;
  ::testing::NiceMock<translate::testing::MockTranslateClient>
      mock_translate_client_;
  MockLanguageModel mock_language_model_;
  std::unique_ptr<TranslateManager> translate_manager_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Target language comes from application locale if the locale's language
// is supported.
TEST_F(TranslateManagerTest, GetTargetLanguageDefaultsToAppLocale) {
  // Ensure the locale is set to a supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));
  manager_->set_application_locale("en");
  EXPECT_EQ("en",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  // Try a second supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("de"));
  manager_->set_application_locale("de");
  EXPECT_EQ("de",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  // Try a those case of non standard code.
  // 'he', 'fil', 'nb' => 'iw', 'tl', 'no'
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("iw"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("he"));
  manager_->set_application_locale("he");
  EXPECT_EQ("iw",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("tl"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("fil"));
  manager_->set_application_locale("fil");
  EXPECT_EQ("tl",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("no"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("nb"));
  manager_->set_application_locale("nb");
  EXPECT_EQ("no",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));
}

// Test that the language model is used if provided.
TEST_F(TranslateManagerTest, GetTargetLanguageFromModel) {
  // Try with a single, supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0)};
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(nullptr,
                                                      &mock_language_model_));

  // Try with two supported languages.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("de"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("de", 1.0),
      MockLanguageModel::LanguageDetails("en", 0.5)};
  EXPECT_EQ("de", TranslateManager::GetTargetLanguage(nullptr,
                                                      &mock_language_model_));

  // Try with first supported language lower in the list.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xx"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("xx", 1.0),
      MockLanguageModel::LanguageDetails("en", 0.5)};
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(nullptr,
                                                      &mock_language_model_));

  // Try with no supported languages.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("yy"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("xx", 1.0),
      MockLanguageModel::LanguageDetails("yy", 0.5)};
  EXPECT_EQ(
      "", TranslateManager::GetTargetLanguage(nullptr, &mock_language_model_));

  // Try non standard codes.
  // 'he', 'fil', 'nb' => 'iw', 'tl', 'no'
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("iw"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("he"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("he", 1.0)};
  EXPECT_EQ("iw", TranslateManager::GetTargetLanguage(nullptr,
                                                      &mock_language_model_));

  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("tl"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("fil"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("fil", 1.0)};
  EXPECT_EQ("tl", TranslateManager::GetTargetLanguage(nullptr,
                                                      &mock_language_model_));

  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("no"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("nb"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("nb", 1.0)};
  EXPECT_EQ("no", TranslateManager::GetTargetLanguage(nullptr,
                                                      &mock_language_model_));
}

TEST_F(TranslateManagerTest, DontTranslateOffline) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_.reset(new translate::TranslateManager(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_));

  // The test measures that the "Translate was disabled" exit can only be
  // reached after the early-out tests including IsOffline() passed.
  base::HistogramTester histogram_tester;

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, false);

  translate_manager_->GetLanguageState().LanguageDetermined("de", true);

  // In the offline case, Initiate will early-out before even hitting the API
  // key test.
  network_notifier_.SimulateOffline();
  translate_manager_->InitiateTranslation("de");
  histogram_tester.ExpectTotalCount(kInitiationStatusName, 0);

  // In the online case, InitiateTranslation will proceed past early out tests.
  network_notifier_.SimulateOnline();
  translate_manager_->InitiateTranslation("de");
  histogram_tester.ExpectUniqueSample(
      kInitiationStatusName,
      translate::TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_PREFS,
      1);
}

TEST_F(TranslateManagerTest, TestRecordTranslateEvent) {
  PrepareTranslateManager();
  const std::string locale = "zh-TW";
  const std::string page_lang = "zh-CN";
  metrics::TranslateEventProto expected_tep;
  expected_tep.set_target_language(locale);
  expected_tep.set_source_language(page_lang);
  EXPECT_CALL(
      mock_translate_ranker_,
      RecordTranslateEvent(metrics::TranslateEventProto::USER_ACCEPT, _,
                           Pointee(EqualsTranslateEventProto(expected_tep))))
      .Times(1);

  InitTranslateEvent(page_lang, locale);
  translate_manager_->RecordTranslateEvent(
      metrics::TranslateEventProto::USER_ACCEPT);
}

TEST_F(TranslateManagerTest, TestShouldOverrideDecision) {
  PrepareTranslateManager();
  const int kEventType = 1;
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          kEventType, _,
          Pointee(EqualsTranslateEventProto(metrics::TranslateEventProto()))))
      .WillOnce(Return(false));
  EXPECT_FALSE(translate_manager_->ShouldOverrideDecision(kEventType));

  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          kEventType, _,
          Pointee(EqualsTranslateEventProto(metrics::TranslateEventProto()))))
      .WillOnce(Return(true));
  EXPECT_TRUE(translate_manager_->ShouldOverrideDecision(kEventType));
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_Default) {
  PrepareTranslateManager();
  SetHasLanguageChanged(true);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(false, "en"));
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(true, "en"));
  histogram_tester.ExpectTotalCount(kInitiationStatusName, 0);
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_HasLanguageChangedFalse) {
  PrepareTranslateManager();
  SetHasLanguageChanged(false);
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          metrics::TranslateEventProto::MATCHES_PREVIOUS_LANGUAGE, _, _))
      .WillOnce(Return(false));
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(translate_manager_->ShouldSuppressBubbleUI(false, "en"));
  histogram_tester.ExpectUniqueSample(
      kInitiationStatusName,
      translate::TranslateBrowserMetrics::
          INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE,
      1);

  EXPECT_CALL(mock_translate_ranker_, ShouldOverrideDecision(_, _, _))
      .WillOnce(Return(false));

  EXPECT_TRUE(translate_manager_->ShouldSuppressBubbleUI(true, "en"));
  histogram_tester.ExpectUniqueSample(
      kInitiationStatusName,
      translate::TranslateBrowserMetrics::
          INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE,
      2);
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_NewUI) {
  PrepareTranslateManager();
  base::test::ScopedFeatureList scoped_feature_list;
  base::HistogramTester histogram_tester;
  scoped_feature_list.InitAndEnableFeature(translate::kTranslateUI2016Q2);
  SetHasLanguageChanged(false);
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(false, "en"));
  histogram_tester.ExpectTotalCount(kInitiationStatusName, 0);
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_IsTooOftenDenied) {
  PrepareTranslateManager();
  SetHasLanguageChanged(true);
  SetLanguageTooOftenDenied("en");
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          metrics::TranslateEventProto::LANGUAGE_DISABLED_BY_AUTO_BLACKLIST, _,
          _))
      .WillOnce(Return(false));
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(translate_manager_->ShouldSuppressBubbleUI(false, "en"));
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(false, "de"));
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(true, "en"));
  histogram_tester.ExpectUniqueSample(
      kInitiationStatusName,
      translate::TranslateBrowserMetrics::
          INITIATION_STATUS_ABORTED_BY_TOO_OFTEN_DENIED,
      1);
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_Override) {
  PrepareTranslateManager();
  base::HistogramTester histogram_tester;
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          metrics::TranslateEventProto::MATCHES_PREVIOUS_LANGUAGE, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          metrics::TranslateEventProto::LANGUAGE_DISABLED_BY_AUTO_BLACKLIST, _,
          _))
      .WillOnce(Return(true));
  SetHasLanguageChanged(false);
  SetLanguageTooOftenDenied("en");
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(false, "en"));
  histogram_tester.ExpectTotalCount(kInitiationStatusName, 0);
}

TEST_F(TranslateManagerTest, RecordInitilizationError) {
  PrepareTranslateManager();
  const std::string target_lang = "en";
  const std::string source_lang = "zh";
  metrics::TranslateEventProto expected_tep;
  expected_tep.set_target_language(target_lang);
  expected_tep.set_source_language(source_lang);
  EXPECT_CALL(
      mock_translate_ranker_,
      RecordTranslateEvent(metrics::TranslateEventProto::INITIALIZATION_ERROR,
                           _, Pointee(EqualsTranslateEventProto(expected_tep))))
      .Times(1);

  InitTranslateEvent(source_lang, target_lang);
  translate_manager_->PageTranslated(source_lang, target_lang,
                                     TranslateErrors::INITIALIZATION_ERROR);
}

}  // namespace testing

}  // namespace translate
