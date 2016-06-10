// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

namespace {

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
    return base::WrapUnique(new TranslatePrefs(prefs_, kAcceptLanguages,
                                               kLanguagePreferredLanguages));
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

class TranslateManagerTest : public ::testing::Test {
 protected:
  TranslateManagerTest()
      : translate_prefs_(&prefs_,
                         kAcceptLanguages,
                         kLanguagePreferredLanguages),
        manager_(TranslateDownloadManager::GetInstance()),
        mock_translate_client_(&driver_, &prefs_) {}

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
  user_prefs::TestingPrefServiceSyncable prefs_;

  // TODO(groby): request TranslatePrefs from |mock_translate_client_| instead.
  TranslatePrefs translate_prefs_;
  TranslateDownloadManager* manager_;

  TestNetworkChangeNotifier network_notifier_;
  translate::testing::MockTranslateDriver driver_;
  ::testing::NiceMock<MockTranslateClient> mock_translate_client_;
  std::unique_ptr<TranslateManager> translate_manager_;

  void TearDown() override { manager_->ResetForTesting(); }
};

}  // namespace

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

}  // namespace translate
