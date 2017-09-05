// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/driver/fake_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class TestSyncService : public syncer::FakeSyncService {
 public:
  TestSyncService()
      : can_sync_start_(true),
        preferred_data_types_(syncer::ModelTypeSet::All()),
        is_engine_initialized_(true),
        is_using_secondary_passphrase_(false) {}

  bool CanSyncStart() const override { return can_sync_start_; }

  syncer::ModelTypeSet GetPreferredDataTypes() const override {
    return preferred_data_types_;
  }

  bool IsEngineInitialized() const override { return is_engine_initialized_; }

  bool IsUsingSecondaryPassphrase() const override {
    return is_using_secondary_passphrase_;
  }

  void SetCanSyncStart(bool can_sync_start) {
    can_sync_start_ = can_sync_start;
  }

  void SetPreferredDataTypes(syncer::ModelTypeSet preferred_data_types) {
    preferred_data_types_ = preferred_data_types;
  }

  void SetIsEngineInitialized(bool is_engine_initialized) {
    is_engine_initialized_ = is_engine_initialized;
  }

  void SetIsUsingSecondaryPassphrase(bool is_using_secondary_passphrase) {
    is_using_secondary_passphrase_ = is_using_secondary_passphrase;
  }

 private:
  bool can_sync_start_;
  syncer::ModelTypeSet preferred_data_types_;
  bool is_engine_initialized_;
  bool is_using_secondary_passphrase_;
};

}  // namespace

class AutofillExperimentsTest : public testing::Test {
 public:
  AutofillExperimentsTest() {}

 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillWalletImportEnabled, true);
  }

  bool IsCreditCardUploadEnabled() {
    return IsCreditCardUploadEnabled("john.smith@gmail.com", "Default");
  }

  bool IsCreditCardUploadEnabled(const std::string& user_email) {
    return IsCreditCardUploadEnabled(user_email, "Default");
  }

  bool IsCreditCardUploadEnabled(const std::string& user_email,
                                 const std::string& field_trial_value) {
    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial("OfferUploadCreditCards",
                                           field_trial_value);

    return autofill::IsCreditCardUploadEnabled(&pref_service_, &sync_service_,
                                               user_email);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  TestSyncService sync_service_;
};

TEST_F(AutofillExperimentsTest, DenyUpload_SyncServiceCannotStart) {
  sync_service_.SetCanSyncStart(false);
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest,
       DenyUpload_SyncServiceDoesNotHaveAutofillProfilePreferredDataType) {
  sync_service_.SetPreferredDataTypes(syncer::ModelTypeSet());
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest, DenyUpload_SyncServiceEngineNotInitialized) {
  sync_service_.SetIsEngineInitialized(false);
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest,
       DenyUpload_SyncServiceUsingSecondaryPassphrase) {
  sync_service_.SetIsUsingSecondaryPassphrase(true);
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest,
       DenyUpload_AutofillWalletImportEnabledPrefIsDisabled) {
  pref_service_.SetBoolean(prefs::kAutofillWalletImportEnabled, false);
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest, DenyUpload_EmptyUserEmail) {
  EXPECT_FALSE(IsCreditCardUploadEnabled(""));
}

TEST_F(AutofillExperimentsTest, AllowUpload_UserEmailWithGoogleDomain) {
  EXPECT_TRUE(IsCreditCardUploadEnabled("john.smith@gmail.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("googler@google.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("old.school@googlemail.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("code.committer@chromium.org"));
}

TEST_F(AutofillExperimentsTest, DenyUpload_UserEmailWithNonGoogleDomain) {
  EXPECT_FALSE(IsCreditCardUploadEnabled("cool.user@hotmail.com"));
  EXPECT_FALSE(IsCreditCardUploadEnabled("john.smith@johnsmith.com"));
  EXPECT_FALSE(IsCreditCardUploadEnabled("fake.googler@google.net"));
  EXPECT_FALSE(IsCreditCardUploadEnabled("fake.committer@chromium.com"));
}

TEST_F(AutofillExperimentsTest,
       AllowUpload_UserEmailWithNonGoogleDomainIfExperimentEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      kAutofillUpstreamAllowAllEmailDomains);
  EXPECT_TRUE(IsCreditCardUploadEnabled("cool.user@hotmail.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("john.smith@johnsmith.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("fake.googler@google.net"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("fake.committer@chromium.com"));
}

TEST_F(AutofillExperimentsTest,
       AllowUpload_CommandLineSwitchOnEvenIfGroupDisabled) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableOfferUploadCreditCards);
  EXPECT_TRUE(IsCreditCardUploadEnabled("john.smith@gmail.com", "Disabled"));
}

TEST_F(AutofillExperimentsTest, DenyUpload_CommandLineSwitchOff) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableOfferUploadCreditCards);
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest, DenyUpload_GroupNameEmpty) {
  EXPECT_FALSE(IsCreditCardUploadEnabled("john.smith@gmail.com", ""));
}

TEST_F(AutofillExperimentsTest, DenyUpload_GroupNameDisabled) {
  EXPECT_FALSE(IsCreditCardUploadEnabled("john.smith@gmail.com", "Disabled"));
}

TEST_F(AutofillExperimentsTest, DenyUpload_GroupNameAnythingButDisabled) {
  EXPECT_TRUE(IsCreditCardUploadEnabled("john.smith@gmail.com", "Enabled"));
}

}  // namespace autofill
