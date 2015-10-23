// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/sync_driver/fake_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kBrandingExperimentName[] = "PasswordBranding";
const char kSmartLockBrandingGroupName[] = "SmartLockBranding";
const char kSmartLockNoBrandingGroupName[] = "NoSmartLockBranding";

enum CustomPassphraseState { NO_CUSTOM_PASSPHRASE, CUSTOM_PASSPHRASE };

enum Branding { NO_BRANDING, BRANDING };

enum SavePromptFirstRunExperience {
  NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE,
  SAVE_PROMPT_FIRST_RUN_EXPERIENCE
};

enum SmartLockUser { SMARTLOCK_USER, NOT_SMARTLOCK_USER };

struct IsSmartLockBrandingEnabledTestcase {
  CustomPassphraseState passphrase_state;
  syncer::ModelType type;
  Branding branding;
  SmartLockUser user_type;
};

struct ShouldShowSavePromptFirstRunExperienceTestcase {
  CustomPassphraseState passphrase_state;
  syncer::ModelType type;
  bool pref_value;
  SavePromptFirstRunExperience first_run_experience;
};

class TestSyncService : public sync_driver::FakeSyncService {
 public:
  TestSyncService() {}

  ~TestSyncService() override {}

  // FakeSyncService overrides.
  bool IsSyncAllowed() const override { return true; }

  bool HasSyncSetupCompleted() const override { return true; }

  bool IsSyncActive() const override { return true; }

  syncer::ModelTypeSet GetActiveDataTypes() const override { return type_set_; }

  syncer::ModelTypeSet GetPreferredDataTypes() const override {
    return type_set_;
  }

  bool IsUsingSecondaryPassphrase() const override {
    return is_using_secondary_passphrase_;
  }

  void SetIsUsingSecondaryPassphrase(bool is_using_secondary_passphrase) {
    is_using_secondary_passphrase_ = is_using_secondary_passphrase;
  }

  void SetActiveDataTypes(syncer::ModelTypeSet type_set) {
    type_set_ = type_set;
  }

  void ClearActiveDataTypes() { type_set_.Clear(); }

  bool CanSyncStart() const override { return true; }

 private:
  syncer::ModelTypeSet type_set_;
  bool is_using_secondary_passphrase_;
};

}  // namespace

class PasswordManagerPasswordBubbleExperimentTest : public testing::Test {
 public:
  PasswordManagerPasswordBubbleExperimentTest() : field_trial_list_(nullptr) {}

  void SetUp() override {
    password_bubble_experiment::RegisterPrefs(pref_service_.registry());
  }

  PrefService* prefs() { return &pref_service_; }

  void EnforceExperimentGroup(const char* name) {
    ASSERT_TRUE(
        base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName, name));
  }

  TestSyncService* sync_service() { return &fake_sync_service_; }

  void TestIsSmartLockBrandingEnabledTestcase(
      const IsSmartLockBrandingEnabledTestcase& test_case) {
    SetupFakeSyncServiceForTestCase(test_case.type, test_case.passphrase_state);
    bool is_smart_lock_branding_enabled =
        password_bubble_experiment::IsSmartLockBrandingEnabled(sync_service());
    if (test_case.branding == BRANDING) {
      EXPECT_TRUE(is_smart_lock_branding_enabled);
    } else {
      EXPECT_FALSE(is_smart_lock_branding_enabled);
    }

    if (test_case.user_type == SMARTLOCK_USER) {
      EXPECT_TRUE(password_bubble_experiment::IsSmartLockUser(sync_service()));
    } else {
      EXPECT_FALSE(password_bubble_experiment::IsSmartLockUser(sync_service()));
    }
  }

  void TestShouldShowSavePromptFirstRunExperienceTestcase(
      const ShouldShowSavePromptFirstRunExperienceTestcase& test_case) {
    SetupFakeSyncServiceForTestCase(test_case.type, test_case.passphrase_state);
    prefs()->SetBoolean(
        password_manager::prefs::kWasSavePrompFirstRunExperienceShown,
        test_case.pref_value);
    bool should_show_first_run_experience =
        password_bubble_experiment::ShouldShowSavePromptFirstRunExperience(
            sync_service(), prefs());
    if (test_case.first_run_experience == SAVE_PROMPT_FIRST_RUN_EXPERIENCE) {
      EXPECT_TRUE(should_show_first_run_experience);
    } else {
      EXPECT_FALSE(should_show_first_run_experience);
    }
  }

 private:
  void SetupFakeSyncServiceForTestCase(syncer::ModelType type,
                                       CustomPassphraseState passphrase_state) {
    syncer::ModelTypeSet active_types;
    active_types.Put(type);
    sync_service()->ClearActiveDataTypes();
    sync_service()->SetActiveDataTypes(active_types);
    sync_service()->SetIsUsingSecondaryPassphrase(passphrase_state ==
                                                  CUSTOM_PASSPHRASE);
  }

  TestSyncService fake_sync_service_;
  base::FieldTrialList field_trial_list_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTest) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {CUSTOM_PASSPHRASE, syncer::PASSWORDS, NO_BRANDING, NOT_SMARTLOCK_USER},
      {CUSTOM_PASSPHRASE, syncer::BOOKMARKS, NO_BRANDING, NOT_SMARTLOCK_USER},
      {NO_CUSTOM_PASSPHRASE, syncer::PASSWORDS, BRANDING, SMARTLOCK_USER},
      {NO_CUSTOM_PASSPHRASE, syncer::BOOKMARKS, NO_BRANDING,
       NOT_SMARTLOCK_USER},
  };

  EnforceExperimentGroup(kSmartLockBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTestNoBranding) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {CUSTOM_PASSPHRASE, syncer::PASSWORDS, NO_BRANDING, NOT_SMARTLOCK_USER},
      {CUSTOM_PASSPHRASE, syncer::BOOKMARKS, NO_BRANDING, NOT_SMARTLOCK_USER},
      {NO_CUSTOM_PASSPHRASE, syncer::PASSWORDS, NO_BRANDING, SMARTLOCK_USER},
      {NO_CUSTOM_PASSPHRASE, syncer::BOOKMARKS, NO_BRANDING,
       NOT_SMARTLOCK_USER},
  };

  EnforceExperimentGroup(kSmartLockNoBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShoulShowSavePrompBrandingGroup) {
  const struct ShouldShowSavePromptFirstRunExperienceTestcase kTestData[] = {
      {CUSTOM_PASSPHRASE, syncer::PASSWORDS, true,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {CUSTOM_PASSPHRASE, syncer::PASSWORDS, false,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {CUSTOM_PASSPHRASE, syncer::BOOKMARKS, true,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {CUSTOM_PASSPHRASE, syncer::BOOKMARKS, false,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {NO_CUSTOM_PASSPHRASE, syncer::PASSWORDS, true,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {NO_CUSTOM_PASSPHRASE, syncer::PASSWORDS, false,
       SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {NO_CUSTOM_PASSPHRASE, syncer::BOOKMARKS, true,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {NO_CUSTOM_PASSPHRASE, syncer::BOOKMARKS, false,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
  };

  EnforceExperimentGroup(kSmartLockBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestShouldShowSavePromptFirstRunExperienceTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShoulShowSavePrompNoBrandingGroup) {
  const struct ShouldShowSavePromptFirstRunExperienceTestcase kTestData[] = {
      {CUSTOM_PASSPHRASE, syncer::PASSWORDS, true,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {CUSTOM_PASSPHRASE, syncer::PASSWORDS, false,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {CUSTOM_PASSPHRASE, syncer::BOOKMARKS, true,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {CUSTOM_PASSPHRASE, syncer::BOOKMARKS, false,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {NO_CUSTOM_PASSPHRASE, syncer::PASSWORDS, true,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {NO_CUSTOM_PASSPHRASE, syncer::PASSWORDS, false,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {NO_CUSTOM_PASSPHRASE, syncer::BOOKMARKS, true,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
      {NO_CUSTOM_PASSPHRASE, syncer::BOOKMARKS, false,
       NO_SAVE_PROMPT_FIRST_RUN_EXPERIENCE},
  };

  EnforceExperimentGroup(kSmartLockNoBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestShouldShowSavePromptFirstRunExperienceTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       RecordFirstRunExperienceWasShownTest) {
  const struct {
    bool initial_pref_value;
    bool result_pref_value;
  } kTestData[] = {
      {false, true}, {true, true},
  };
  for (const auto& test_case : kTestData) {
    // Record Save prompt first run experience.
    prefs()->SetBoolean(
        password_manager::prefs::kWasSavePrompFirstRunExperienceShown,
        test_case.initial_pref_value);
    password_bubble_experiment::RecordSavePromptFirstRunExperienceWasShown(
        prefs());
    EXPECT_EQ(
        test_case.result_pref_value,
        prefs()->GetBoolean(
            password_manager::prefs::kWasSavePrompFirstRunExperienceShown));
    // Record Auto sign-in first run experience.
    prefs()->SetBoolean(
        password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
        test_case.initial_pref_value);
    EXPECT_EQ(!test_case.initial_pref_value,
              password_bubble_experiment::
                  ShouldShowAutoSignInPromptFirstRunExperience(prefs()));
    password_bubble_experiment::
        RecordAutoSignInPromptFirstRunExperienceWasShown(prefs());
    EXPECT_EQ(
        test_case.result_pref_value,
        prefs()->GetBoolean(
            password_manager::prefs::kWasAutoSignInFirstRunExperienceShown));
    EXPECT_EQ(!test_case.result_pref_value,
              password_bubble_experiment::
                  ShouldShowAutoSignInPromptFirstRunExperience(prefs()));
  }
}
