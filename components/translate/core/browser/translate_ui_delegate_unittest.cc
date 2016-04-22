// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ui_delegate.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Return;
using testing::Test;
using translate::testing::MockTranslateDriver;


namespace translate {

#if defined(OS_CHROMEOS)
const char* preferred_languages_prefs =
    "settings.language.preferred_languages";
#else
const char* preferred_languages_prefs = NULL;
#endif

class MockTranslateClient : public TranslateClient {
 public:
  MockTranslateClient(
      TranslateDriver* driver,
      PrefService* prefs)
      : driver_(driver),
        prefs_(prefs) {
  }

  TranslateDriver* GetTranslateDriver() { return driver_; }
  PrefService* GetPrefs() { return prefs_; }

  std::unique_ptr<TranslatePrefs> GetTranslatePrefs() {
    return base::WrapUnique(new TranslatePrefs(prefs_, "intl.accept_languages",
                                               preferred_languages_prefs));
  }

  MOCK_METHOD0(GetTranslateAcceptLanguages, TranslateAcceptLanguages*());
  MOCK_CONST_METHOD0(GetInfobarIconID, int());

  MOCK_CONST_METHOD1(CreateInfoBarMock,
                     infobars::InfoBar*(TranslateInfoBarDelegate*));
  std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<TranslateInfoBarDelegate> delegate) const {
    return base::WrapUnique(CreateInfoBarMock(std::move(delegate).get()));
  }

  MOCK_METHOD5(ShowTranslateUI, void(translate::TranslateStep,
                                     const std::string&,
                                     const std::string&,
                                     TranslateErrors::Type,
                                     bool));
  MOCK_METHOD1(IsTranslatableURL, bool(const GURL&));
  MOCK_METHOD1(ShowReportLanguageDetectionErrorUI, void(const GURL&));

 private:
  TranslateDriver* driver_;
  PrefService* prefs_;
};

class TranslateUIDelegateTest : public ::testing::Test {
 public:
  TranslateUIDelegateTest() : ::testing::Test() {}

  void SetUp() override {
    pref_service_.reset(new user_prefs::TestingPrefServiceSyncable());
    TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());

    client_.reset(new MockTranslateClient(
        &driver_,
        pref_service_.get()));

    manager_.reset(new TranslateManager(client_.get(), "hi"));
    manager_->GetLanguageState().set_translation_declined(false);

    delegate_.reset(new TranslateUIDelegate(
        manager_->GetWeakPtr(), "ar", "fr"));

    ASSERT_FALSE(client_->GetTranslatePrefs()->IsTooOftenDenied("ar"));
  }


  MockTranslateDriver driver_;
  std::unique_ptr<MockTranslateClient> client_;
  std::unique_ptr<user_prefs::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<TranslateManager> manager_;
  std::unique_ptr<TranslateUIDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TranslateUIDelegateTest);
};


TEST_F(TranslateUIDelegateTest, CheckDeclinedFalse) {
  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  for (int i = 0; i < 10; i++) {
    prefs->IncrementTranslationAcceptedCount("ar");
  }
  prefs->IncrementTranslationDeniedCount("ar");
  int accepted_count = prefs->GetTranslationAcceptedCount("ar");
  int denied_count = prefs->GetTranslationDeniedCount("ar");

  delegate_->TranslationDeclined(false);

  EXPECT_EQ(accepted_count, prefs->GetTranslationAcceptedCount("ar"));
  EXPECT_EQ(denied_count, prefs->GetTranslationDeniedCount("ar"));
  EXPECT_FALSE(prefs->IsTooOftenDenied("ar"));
  EXPECT_FALSE(manager_->GetLanguageState().translation_declined());
}

TEST_F(TranslateUIDelegateTest, CheckDeclinedTrue) {
  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  for (int i = 0; i < 10; i++) {
    prefs->IncrementTranslationAcceptedCount("ar");
  }
  prefs->IncrementTranslationDeniedCount("ar");
  int denied_count = prefs->GetTranslationDeniedCount("ar");

  delegate_->TranslationDeclined(true);

  EXPECT_EQ(0, prefs->GetTranslationAcceptedCount("ar"));
  EXPECT_EQ(denied_count + 1, prefs->GetTranslationDeniedCount("ar"));
  EXPECT_TRUE(manager_->GetLanguageState().translation_declined());
}

// TODO(ftang) Currently this file only test TranslationDeclined(), we
// need to add the test for other functions soon to increase the test
// coverage.

}  // namespace translate
