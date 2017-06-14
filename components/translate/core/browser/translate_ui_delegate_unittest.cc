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
#include "components/metrics/proto/translate_event.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Return;
using testing::Test;
using translate::testing::MockTranslateClient;
using translate::testing::MockTranslateDriver;
using translate::testing::MockTranslateRanker;

namespace translate {

class TranslateUIDelegateTest : public ::testing::Test {
 public:
  TranslateUIDelegateTest() : ::testing::Test() {}

  void SetUp() override {
    pref_service_.reset(new sync_preferences::TestingPrefServiceSyncable());
    pref_service_->registry()->RegisterStringPref(
        "settings.language.preferred_languages", std::string());
    pref_service_->registry()->RegisterStringPref("intl.accept_languages",
                                                  std::string());
    TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());

    client_.reset(new MockTranslateClient(&driver_, pref_service_.get()));
    ranker_.reset(new MockTranslateRanker());
    manager_.reset(new TranslateManager(client_.get(), ranker_.get(), "hi"));
    manager_->GetLanguageState().set_translation_declined(false);

    delegate_.reset(
        new TranslateUIDelegate(manager_->GetWeakPtr(), "ar", "fr"));

    ASSERT_FALSE(client_->GetTranslatePrefs()->IsTooOftenDenied("ar"));
  }

  // Do not reorder. These are ordered for dependency on creation/destruction.
  MockTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<MockTranslateClient> client_;
  std::unique_ptr<MockTranslateRanker> ranker_;
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
  int ignored_count = prefs->GetTranslationIgnoredCount("ar");

  delegate_->TranslationDeclined(false);

  EXPECT_EQ(accepted_count, prefs->GetTranslationAcceptedCount("ar"));
  EXPECT_EQ(denied_count, prefs->GetTranslationDeniedCount("ar"));
  EXPECT_EQ(ignored_count + 1, prefs->GetTranslationIgnoredCount("ar"));
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
  int ignored_count = prefs->GetTranslationIgnoredCount("ar");

  delegate_->TranslationDeclined(true);

  EXPECT_EQ(0, prefs->GetTranslationAcceptedCount("ar"));
  EXPECT_EQ(denied_count + 1, prefs->GetTranslationDeniedCount("ar"));
  EXPECT_EQ(ignored_count, prefs->GetTranslationIgnoredCount("ar"));
  EXPECT_TRUE(manager_->GetLanguageState().translation_declined());
}

TEST_F(TranslateUIDelegateTest, SetLanguageBlocked) {
  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  manager_->GetLanguageState().SetTranslateEnabled(true);
  EXPECT_TRUE(manager_->GetLanguageState().translate_enabled());
  prefs->UnblockLanguage("ar");
  EXPECT_FALSE(prefs->IsBlockedLanguage("ar"));

  delegate_->SetLanguageBlocked(true);

  EXPECT_TRUE(prefs->IsBlockedLanguage("ar"));
  EXPECT_FALSE(manager_->GetLanguageState().translate_enabled());

  // Reset it to true again after delegate_->SetLanguageBlocked(true)
  // turn it to false.
  manager_->GetLanguageState().SetTranslateEnabled(true);

  delegate_->SetLanguageBlocked(false);

  EXPECT_FALSE(prefs->IsBlockedLanguage("ar"));
  EXPECT_TRUE(manager_->GetLanguageState().translate_enabled());
}

TEST_F(TranslateUIDelegateTest, ShouldAlwaysTranslateBeCheckedByDefaultNever) {
  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  prefs->ResetTranslationAcceptedCount("ar");

  for (int i = 0; i < 100; i++) {
    EXPECT_FALSE(delegate_->ShouldAlwaysTranslateBeCheckedByDefault());
    prefs->IncrementTranslationAcceptedCount("ar");
  }
}

TEST_F(TranslateUIDelegateTest, ShouldAlwaysTranslateBeCheckedByDefault2) {
  const char kGroupName[] = "GroupA";
  std::map<std::string, std::string> params = {
      {kAlwaysTranslateOfferThreshold, "2"}};
  variations::AssociateVariationParams(translate::kTranslateUI2016Q2TrialName,
                                       kGroupName, params);
  // need a trial_list to init the global instance used internally
  // inside CreateFieldTrial().
  base::FieldTrialList trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(translate::kTranslateUI2016Q2TrialName,
                                         kGroupName);

  std::unique_ptr<TranslatePrefs> prefs(client_->GetTranslatePrefs());
  prefs->ResetTranslationAcceptedCount("ar");

  for (int i = 0; i < 2; i++) {
    EXPECT_FALSE(delegate_->ShouldAlwaysTranslateBeCheckedByDefault());
    prefs->IncrementTranslationAcceptedCount("ar");
  }
  EXPECT_TRUE(delegate_->ShouldAlwaysTranslateBeCheckedByDefault());
  prefs->IncrementTranslationAcceptedCount("ar");

  EXPECT_FALSE(delegate_->ShouldAlwaysTranslateBeCheckedByDefault());
}

// TODO(ftang) Currently this file only test TranslationDeclined(), we
// need to add the test for other functions soon to increase the test
// coverage.

}  // namespace translate
