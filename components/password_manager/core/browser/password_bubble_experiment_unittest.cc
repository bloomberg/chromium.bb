// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include "components/sync_driver/fake_sync_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kBrandingExperimentName[] = "PasswordBranding";
const char kSmartLockBrandingGroupName[] = "SmartLockBranding";
const char kSmartLockNoBrandingGroupName[] = "NoSmartLockBranding";

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
  PasswordManagerPasswordBubbleExperimentTest()
      : field_trial_list_(new metrics::SHA1EntropyProvider("foo")) {}

  void TearDown() override { variations::testing::ClearAllVariationParams(); }

  void EnforceExperimentGroup(const char* name) {
    ASSERT_TRUE(
        base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName, name));
  }

  TestSyncService* sync_service() { return &fake_sync_service_; }

 protected:
  TestSyncService fake_sync_service_;
  base::FieldTrialList field_trial_list_;
};

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTest) {
  const struct {
    bool is_using_secondary_passphrase;
    syncer::ModelType type;
    bool is_branding_enabled;
  } kTestData[] = {
      {true, syncer::PASSWORDS, false},
      {false, syncer::PASSWORDS, true},
      {true, syncer::BOOKMARKS, false},
      {false, syncer::BOOKMARKS, false},
  };

  EnforceExperimentGroup(kSmartLockBrandingGroupName);
  for (const auto& test_case : kTestData) {
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
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTest1) {
  const struct {
    bool is_using_secondary_passphrase;
    syncer::ModelType type;
    bool is_branding_enabled;
  } kTestData[] = {
      {true, syncer::PASSWORDS, false},
      {false, syncer::PASSWORDS, false},
      {true, syncer::BOOKMARKS, false},
      {false, syncer::BOOKMARKS, false},
  };

  EnforceExperimentGroup(kSmartLockNoBrandingGroupName);
  for (const auto& test_case : kTestData) {
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
}
