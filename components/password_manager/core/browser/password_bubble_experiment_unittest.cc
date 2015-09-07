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

struct IsSmartLockBrandingEnabledTestcase {
  bool is_using_secondary_passphrase;
  syncer::ModelType type;
  bool is_branding_enabled;
};

struct ShouldShowSavePromptFirstRunExperienceTestcase {
  bool is_using_secondary_passphrase;
  syncer::ModelType type;
  bool pref_value;
  bool is_branding_enabled;
};

class TestSyncService : public sync_driver::FakeSyncService {
 public:
  TestSyncService(){};

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
    syncer::ModelTypeSet active_types;
    active_types.Put(test_case.type);
    sync_service()->ClearActiveDataTypes();
    sync_service()->SetActiveDataTypes(active_types);
    sync_service()->SetIsUsingSecondaryPassphrase(
        test_case.is_using_secondary_passphrase);
    EXPECT_EQ(
        test_case.is_branding_enabled,
        password_bubble_experiment::IsSmartLockBrandingEnabled(sync_service()));
  }

  void TestShouldShowSavePromptFirstRunExperienceTestcase(
      const ShouldShowSavePromptFirstRunExperienceTestcase& test_case) {
    syncer::ModelTypeSet active_types;
    active_types.Put(test_case.type);
    sync_service()->ClearActiveDataTypes();
    sync_service()->SetActiveDataTypes(active_types);
    sync_service()->SetIsUsingSecondaryPassphrase(
        test_case.is_using_secondary_passphrase);
    prefs()->SetBoolean(
        password_manager::prefs::kWasSavePrompFirstRunExperienceShown,
        test_case.pref_value);
    EXPECT_EQ(
        test_case.is_branding_enabled,
        password_bubble_experiment::ShouldShowSavePromptFirstRunExperience(
            sync_service(), prefs()));
  }

 protected:
  TestSyncService fake_sync_service_;
  base::FieldTrialList field_trial_list_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTest) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {true, syncer::PASSWORDS, false},
      {false, syncer::PASSWORDS, true},
      {true, syncer::BOOKMARKS, false},
      {false, syncer::BOOKMARKS, false},
  };

  EnforceExperimentGroup(kSmartLockBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTestNoBranding) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {true, syncer::PASSWORDS, false},
      {false, syncer::PASSWORDS, false},
      {true, syncer::BOOKMARKS, false},
      {false, syncer::BOOKMARKS, false},
  };

  EnforceExperimentGroup(kSmartLockNoBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShoulShowSavePrompBrandingGroup) {
  const struct ShouldShowSavePromptFirstRunExperienceTestcase kTestData[] = {
      {true, syncer::PASSWORDS, true, false},
      {true, syncer::PASSWORDS, false, false},
      {false, syncer::PASSWORDS, true, false},
      {false, syncer::PASSWORDS, false, true},
      {true, syncer::BOOKMARKS, true, false},
      {true, syncer::BOOKMARKS, false, false},
      {false, syncer::BOOKMARKS, true, false},
      {false, syncer::BOOKMARKS, false, false},
  };

  EnforceExperimentGroup(kSmartLockBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestShouldShowSavePromptFirstRunExperienceTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShoulShowSavePrompNoBrandingGroup) {
  const struct ShouldShowSavePromptFirstRunExperienceTestcase kTestData[] = {
      {true, syncer::PASSWORDS, true, false},
      {true, syncer::PASSWORDS, false, false},
      {false, syncer::PASSWORDS, true, false},
      {false, syncer::PASSWORDS, false, false},
      {true, syncer::BOOKMARKS, true, false},
      {true, syncer::BOOKMARKS, false, false},
      {false, syncer::BOOKMARKS, true, false},
      {false, syncer::BOOKMARKS, false, false},
  };

  EnforceExperimentGroup(kSmartLockNoBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestShouldShowSavePromptFirstRunExperienceTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       RecordSavePromptFirstRunExperienceWasShownTest) {
  const struct {
    bool initial_pref_value;
    bool result_pref_value;
  } kTestData[] = {
      {false, true}, {true, true},
  };
  for (const auto& test_case : kTestData) {
    prefs()->SetBoolean(
        password_manager::prefs::kWasSavePrompFirstRunExperienceShown,
        test_case.initial_pref_value);
    password_bubble_experiment::RecordSavePromptFirstRunExperienceWasShown(
        prefs());
    EXPECT_EQ(
        test_case.result_pref_value,
        prefs()->GetBoolean(
            password_manager::prefs::kWasSavePrompFirstRunExperienceShown));
  }
}
