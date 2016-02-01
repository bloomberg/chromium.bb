// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include <ostream>

#include "base/metrics/field_trial.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_driver/fake_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_bubble_experiment {

namespace {

const char kSmartLockNoBrandingGroupName[] = "NoSmartLockBranding";

enum class CustomPassphraseState { NONE, SET };

enum class SavePromptFirstRunExperience { NONE, PRESENT };

enum class UserType { SMARTLOCK, NOT_SMARTLOCK };

struct IsSmartLockBrandingEnabledTestcase {
  CustomPassphraseState passphrase_state;
  syncer::ModelType type;
  SmartLockBranding expected_branding;
  UserType expected_user_type;
};

std::ostream& operator<<(std::ostream& os,
                         const IsSmartLockBrandingEnabledTestcase& testcase) {
  os << (testcase.passphrase_state == CustomPassphraseState::SET ? "{SET, "
                                                                 : "{NONE, ");
  os << (testcase.type == syncer::PASSWORDS ? "syncer::PASSWORDS, "
                                            : "not syncer::PASSWORDS, ");
  switch (testcase.expected_branding) {
    case SmartLockBranding::NONE:
      os << "NONE, ";
      break;
    case SmartLockBranding::FULL:
      os << "FULL, ";
      break;
    case SmartLockBranding::SAVE_BUBBLE_ONLY:
      os << "SAVE_BUBBLE_ONLY, ";
      break;
  }
  os << (testcase.expected_user_type == UserType::SMARTLOCK ? "SMARTLOCK}"
                                                            : "NOT_SMARTLOCK}");
  return os;
}

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

  bool IsFirstSetupComplete() const override { return true; }

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

  void SetUp() override { RegisterPrefs(pref_service_.registry()); }

  PrefService* prefs() { return &pref_service_; }

  void EnforceExperimentGroup(const char* group_name) {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName,
                                                       group_name));
  }

  TestSyncService* sync_service() { return &fake_sync_service_; }

  void TestIsSmartLockBrandingEnabledTestcase(
      const IsSmartLockBrandingEnabledTestcase& test_case) {
    SetupFakeSyncServiceForTestCase(test_case.type, test_case.passphrase_state);
    EXPECT_EQ(test_case.expected_branding,
              GetSmartLockBrandingState(sync_service()));
    EXPECT_EQ(test_case.expected_user_type == UserType::SMARTLOCK,
              IsSmartLockUser(sync_service()));
  }

  void TestShouldShowSavePromptFirstRunExperienceTestcase(
      const ShouldShowSavePromptFirstRunExperienceTestcase& test_case) {
    SetupFakeSyncServiceForTestCase(test_case.type, test_case.passphrase_state);
    prefs()->SetBoolean(
        password_manager::prefs::kWasSavePrompFirstRunExperienceShown,
        test_case.pref_value);
    bool should_show_first_run_experience =
        ShouldShowSavePromptFirstRunExperience(sync_service(), prefs());
    if (test_case.first_run_experience ==
        SavePromptFirstRunExperience::PRESENT) {
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
                                                  CustomPassphraseState::SET);
  }

  TestSyncService fake_sync_service_;
  base::FieldTrialList field_trial_list_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTestNoBranding) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, SmartLockBranding::NONE,
       UserType::SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
  };

  EnforceExperimentGroup(kSmartLockNoBrandingGroupName);
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("test_case = ") << test_case);
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTest_FULL) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, SmartLockBranding::FULL,
       UserType::SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
  };

  EnforceExperimentGroup(kSmartLockBrandingGroupName);
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("test_case = ") << test_case);
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTest_SAVE_BUBBLE_ONLY) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::PASSWORDS,
       SmartLockBranding::SAVE_BUBBLE_ONLY, UserType::SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
  };

  EnforceExperimentGroup(kSmartLockBrandingSavePromptOnlyGroupName);
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("test_case = ") << test_case);
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShoulShowSavePrompBrandingGroup) {
  const struct ShouldShowSavePromptFirstRunExperienceTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::PASSWORDS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, false,
       SavePromptFirstRunExperience::PRESENT},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, false,
       SavePromptFirstRunExperience::NONE},
  };

  EnforceExperimentGroup(kSmartLockBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestShouldShowSavePromptFirstRunExperienceTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShoulShowSavePrompNoBrandingGroup) {
  const struct ShouldShowSavePromptFirstRunExperienceTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::PASSWORDS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, false,
       SavePromptFirstRunExperience::NONE},
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
    RecordSavePromptFirstRunExperienceWasShown(prefs());
    EXPECT_EQ(
        test_case.result_pref_value,
        prefs()->GetBoolean(
            password_manager::prefs::kWasSavePrompFirstRunExperienceShown));
    // Record Auto sign-in first run experience.
    prefs()->SetBoolean(
        password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
        test_case.initial_pref_value);
    EXPECT_EQ(!test_case.initial_pref_value,
              ShouldShowAutoSignInPromptFirstRunExperience(prefs()));
        RecordAutoSignInPromptFirstRunExperienceWasShown(prefs());
    EXPECT_EQ(
        test_case.result_pref_value,
        prefs()->GetBoolean(
            password_manager::prefs::kWasAutoSignInFirstRunExperienceShown));
    EXPECT_EQ(!test_case.result_pref_value,
              ShouldShowAutoSignInPromptFirstRunExperience(prefs()));
  }
}

}  // namespace password_bubble_experiment
