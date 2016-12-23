// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SetArgPointee;

namespace translate {

namespace {

const char kTrialName[] = "MyTrial";

#if defined(OS_CHROMEOS)
const char kLanguagePreferredLanguages[] =
    "settings.language.preferred_languages";
#else
const char* kLanguagePreferredLanguages = nullptr;
#endif
const char kAcceptLanguages[] = "intl.accept_languages";

// Overrides NetworkChangeNotifier, simulatng connection type changes for tests.
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
    connection_type_to_return_ =net::NetworkChangeNotifier::CONNECTION_NONE;
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

// TODO(groby): Combine with MockTranslateClient in TranslateUiDelegateTest.
class MockTranslateClient : public TranslateClient {
 public:
  MockTranslateClient(TranslateDriver* driver, PrefService* prefs)
      : driver_(driver), prefs_(prefs) {}

  // TODO(groby): Does TranslateClient need a virtual dtor?
  virtual ~MockTranslateClient() {}

  TranslateDriver* GetTranslateDriver() { return driver_; }
  PrefService* GetPrefs() { return prefs_; }

  std::unique_ptr<TranslatePrefs> GetTranslatePrefs() {
    return base::MakeUnique<TranslatePrefs>(prefs_, kAcceptLanguages,
                                            kLanguagePreferredLanguages);
  }
  MOCK_METHOD0(GetTranslateAcceptLanguages, TranslateAcceptLanguages*());
  MOCK_CONST_METHOD0(GetInfobarIconID, int());

#if !defined(USE_AURA)
  MOCK_CONST_METHOD1(CreateInfoBarMock,
                     infobars::InfoBar*(TranslateInfoBarDelegate*));
  std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<TranslateInfoBarDelegate> delegate) const {
    return base::WrapUnique(CreateInfoBarMock(std::move(delegate).get()));
  }
#endif

  MOCK_METHOD5(ShowTranslateUI,
               void(translate::TranslateStep,
                    const std::string&,
                    const std::string&,
                    TranslateErrors::Type,
                    bool));

  MOCK_METHOD1(IsTranslatableURL, bool(const GURL&));
  MOCK_METHOD1(ShowReportLanguageDetectionErrorUI,
               void(const GURL& report_url));

 private:
  TranslateDriver* driver_;
  PrefService* prefs_;
};

}  // namespace

namespace testing {

class TranslateManagerTest : public ::testing::Test {
 protected:
  TranslateManagerTest()
      : translate_prefs_(&prefs_,
                         kAcceptLanguages,
                         kLanguagePreferredLanguages),
        manager_(TranslateDownloadManager::GetInstance()),
        mock_translate_client_(&driver_, &prefs_),
        field_trial_list_(new base::FieldTrialList(nullptr)) {}

  void SetUp() override {
    // Ensure we're not requesting a server-side translate language list.
    TranslateLanguageList::DisableUpdate();
    prefs_.registry()->RegisterStringPref(kAcceptLanguages, std::string());
#if defined(OS_CHROMEOS)
    prefs_.registry()->RegisterStringPref(kLanguagePreferredLanguages,
                                          std::string());
#endif
    TranslatePrefs::RegisterProfilePrefs(prefs_.registry());
    // TODO(groby): Figure out RegisterProfilePrefs() should register this.
    prefs_.registry()->RegisterBooleanPref(
        prefs::kEnableTranslate, true,
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
        &mock_translate_client_, kAcceptLanguages));
  }

  // Prepare the test for ULP related tests.
  // Put the ulp json into profile.
  void PrepareULPTest(const char* ulp_json, bool turn_on_feature) {
    PrepareTranslateManager();
    std::unique_ptr<base::Value> profile(CreateProfileFromJSON(ulp_json));
    prefs_.SetUserPref(TranslatePrefs::kPrefLanguageProfile, profile.release());
    if (turn_on_feature)
      TurnOnTranslateByULP();
  }

  std::unique_ptr<base::Value> CreateProfileFromJSON(const char* json) {
    int error_code = 0;
    std::string error_msg;
    int error_line = 0;
    int error_column = 0;

    std::unique_ptr<base::Value> profile(base::JSONReader::ReadAndReturnError(
        json, 0, &error_code, &error_msg, &error_line, &error_column));

    EXPECT_EQ(0, error_code) << error_msg << " at " << error_line << ":"
                             << error_column << std::endl
                             << json;
    return profile;
  }

  void TurnOnTranslateByULP() {
    scoped_refptr<base::FieldTrial> trial(
        CreateFieldTrial(kTrialName, 100, "Enabled", NULL));
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(
        translate::kTranslateLanguageByULP.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  scoped_refptr<base::FieldTrial> CreateFieldTrial(
      const std::string& trial_name,
      int total_probability,
      const std::string& default_group_name,
      int* default_group_number) {
    return base::FieldTrialList::FactoryGetFieldTrial(
        trial_name, total_probability, default_group_name,
        base::FieldTrialList::kNoExpirationYear, 1, 1,
        base::FieldTrial::SESSION_RANDOMIZED, default_group_number);
  }

  // Functions to help TEST_F in subclass to access private functions in
  // TranslteManager so we can unit test them.
  std::string CallGetTargetLanguageFromULP() {
    return TranslateManager::GetTargetLanguageFromULP(&translate_prefs_);
  }
  bool CallLanguageInULP(const std::string& language) {
    return translate_manager_->LanguageInULP(language);
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;

  // TODO(groby): request TranslatePrefs from |mock_translate_client_| instead.
  TranslatePrefs translate_prefs_;
  TranslateDownloadManager* manager_;

  TestNetworkChangeNotifier network_notifier_;
  translate::testing::MockTranslateDriver driver_;
  ::testing::NiceMock<MockTranslateClient> mock_translate_client_;
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
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(&translate_prefs_));

  // Try a second supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("de"));
  manager_->set_application_locale("de");
  EXPECT_EQ("de", TranslateManager::GetTargetLanguage(&translate_prefs_));
}

// If the application locale's language is not supported, the target language
// falls back to the first supported language in |accept_languages_list|. If
// none of the languages in |accept_language_list| is supported, the target
// language is empty.
TEST_F(TranslateManagerTest, GetTargetLanguageAcceptLangFallback) {
  std::vector<std::string> accept_language_list;

  // Ensure locale is set to a not-supported language.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xy"));
  manager_->set_application_locale("xy");

  // Default return is empty string.
  EXPECT_EQ("", TranslateManager::GetTargetLanguage(&translate_prefs_));

  // Unsupported languages still result in the empty string.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("zy"));
  accept_language_list.push_back("zy");
  translate_prefs_.UpdateLanguageList(accept_language_list);
  EXPECT_EQ("", TranslateManager::GetTargetLanguage(&translate_prefs_));

  // First supported language is the fallback language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));
  accept_language_list.push_back("en");
  translate_prefs_.UpdateLanguageList(accept_language_list);
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(&translate_prefs_));
}

TEST_F(TranslateManagerTest, DontTranslateOffline) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_.reset(new translate::TranslateManager(
      &mock_translate_client_, kAcceptLanguages));

  // The test measures that the "Translate was disabled" exit can only be
  // reached after the early-out tests including IsOffline() passed.
  const char kMetricName[] = "Translate.InitiationStatus.v2";
  base::HistogramTester histogram_tester;

  prefs_.SetBoolean(prefs::kEnableTranslate, false);

  translate_manager_->GetLanguageState().LanguageDetermined("de", true);

  // In the offline case, Initiate will early-out before even hitting the API
  // key test.
  network_notifier_.SimulateOffline();
  translate_manager_->InitiateTranslation("de");
  histogram_tester.ExpectTotalCount(kMetricName, 0);

  // In the online case, InitiateTranslation will proceed past early out tests.
  network_notifier_.SimulateOnline();
  translate_manager_->InitiateTranslation("de");
  histogram_tester.ExpectUniqueSample(
      kMetricName,
      translate::TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_PREFS,
      1);
}

// Utility function to set the threshold params
void ChangeThresholdInParams(
    const char* initiate_translation_confidence_threshold,
    const char* initiate_translation_probability_threshold,
    const char* target_language_confidence_threshold,
    const char* target_language_probability_threshold) {
  ASSERT_TRUE(variations::AssociateVariationParams(
      kTrialName, "Enabled", {{"initiate_translation_ulp_confidence_threshold",
                               initiate_translation_confidence_threshold},
                              {"initiate_translation_ulp_probability_threshold",
                               initiate_translation_probability_threshold},
                              {"target_language_ulp_confidence_threshold",
                               target_language_confidence_threshold},
                              {"target_language_ulp_probability_threshold",
                               target_language_probability_threshold}}));
}

// Normal ULP in Json
const char ulp_1[] =
    "{\n"
    "  \"reading\": {\n"
    "    \"confidence\": 0.8,\n"
    "    \"preference\": [\n"
    "      {\n"
    "        \"language\": \"fr\",\n"
    "        \"probability\": 0.6\n"
    "      }, {\n"
    "        \"language\": \"pt-PT\",\n"
    "        \"probability\": 0.4\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "}";

// ULP in Json with smaller probability of several es-* language codes
// sum up to 0.7.
const char ulp_2[] =
    "{\n"
    "  \"reading\": {\n"
    "    \"confidence\": 0.9,\n"
    "    \"preference\": [\n"
    "      {\n"
    "        \"language\": \"fr\",\n"
    "        \"probability\": 0.3\n"
    "      }, {\n"
    "        \"language\": \"es-419\",\n"
    "        \"probability\": 0.2\n"
    "      }, {\n"
    "        \"language\": \"es-MX\",\n"
    "        \"probability\": 0.2\n"
    "      }, {\n"
    "        \"language\": \"es-US\",\n"
    "        \"probability\": 0.2\n"
    "      }, {\n"
    "        \"language\": \"es-CL\",\n"
    "        \"probability\": 0.1\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "}";

TEST_F(TranslateManagerTest, TestGetTargetLanguageFromULPFeatureOff) {
  PrepareULPTest(ulp_1, false);

  EXPECT_STREQ("", CallGetTargetLanguageFromULP().c_str());
}

TEST_F(TranslateManagerTest, TestGetTargetLanguageFromULPHighConfidence) {
  PrepareULPTest(ulp_1, true);

  // The default hardcoded threshold are confidence: 0.7, probability: 0.55
  EXPECT_STREQ("fr", CallGetTargetLanguageFromULP().c_str());
}

TEST_F(TranslateManagerTest,
       TestGetTargetLanguageFromULPHighConfidenceThresholdFromConfig) {
  PrepareULPTest(ulp_1, true);
  ChangeThresholdInParams("", "", "0.81", "0.5");

  // Should get empty string as result since the confidence threshold is above
  // the ULP (0.8 in the ulp_1).
  EXPECT_STREQ("", CallGetTargetLanguageFromULP().c_str());
}

TEST_F(TranslateManagerTest,
       TestGetTargetLanguageFromULPHighProbabilityThresholdFromConfig) {
  PrepareULPTest(ulp_1, true);
  ChangeThresholdInParams("", "", "0.4", "0.61");

  // Should get empty string as result since the confidence threshold is above
  // the ULP (0.6 for fr in the ulp_1).
  EXPECT_STREQ("", CallGetTargetLanguageFromULP().c_str());
}

TEST_F(TranslateManagerTest, TestGetTargetLanguageFromULPProbabilitySumUp) {
  PrepareULPTest(ulp_2, true);
  ChangeThresholdInParams("", "", "0.4", "0.61");

  // Should get "es" since the sum of the "es-*" probability is 0.7.
  EXPECT_STREQ("es", CallGetTargetLanguageFromULP().c_str());
}

TEST_F(TranslateManagerTest, TestLanguageInULPFeatureOff) {
  PrepareULPTest(ulp_1, false);

  EXPECT_FALSE(CallLanguageInULP("fr"));
  EXPECT_FALSE(CallLanguageInULP("pt"));
  EXPECT_FALSE(CallLanguageInULP("zh-TW"));
}

TEST_F(TranslateManagerTest, TestLanguageInULPDefaultThreshold) {
  PrepareULPTest(ulp_1, true);

  // The default hardcoded threshold are confidence: 0.75, probability: 0.5
  EXPECT_TRUE(CallLanguageInULP("fr"));
  EXPECT_FALSE(CallLanguageInULP("pt"));
  EXPECT_FALSE(CallLanguageInULP("zh-TW"));
}

TEST_F(TranslateManagerTest,
       TestLanguageInULPHighConfidenceThresholdFromConfig) {
  PrepareULPTest(ulp_1, true);
  ChangeThresholdInParams("0.9", "0.5", "", "");
  // "fr" and "pt" should return false because the confidence threshold is set
  // to 0.9.
  EXPECT_FALSE(CallLanguageInULP("fr"));
  EXPECT_FALSE(CallLanguageInULP("pt"));
  EXPECT_FALSE(CallLanguageInULP("zh-TW"));
}

TEST_F(TranslateManagerTest,
       TestLanguageInULPLowConfidenceThresholdFromConfig) {
  PrepareULPTest(ulp_1, true);
  ChangeThresholdInParams("0.79", "0.39", "", "");
  // Both "fr" and "pt" should reutrn true because the confidence threshold is
  // 0.79 and lower than 0.8 and the probability threshold is lower than both
  // the one with "fr" (0.6) and "pt-PT" (0.4).
  EXPECT_TRUE(CallLanguageInULP("fr"));
  EXPECT_TRUE(CallLanguageInULP("pt"));
  EXPECT_FALSE(CallLanguageInULP("zh-TW"));
}

}  // namespace testing

}  // namespace translate
