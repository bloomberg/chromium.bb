// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillExperimentsTest : public testing::Test {
 public:
  AutofillExperimentsTest() {}

 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillWalletImportEnabled, true);
  }

  bool IsCreditCardUploadEnabled() {
    return IsCreditCardUploadEnabled("john.smith@gmail.com");
  }

  bool IsCreditCardUploadEnabled(const std::string& user_email) {
    return autofill::IsCreditCardUploadEnabled(&pref_service_, &sync_service_,
                                               user_email);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
};

TEST_F(AutofillExperimentsTest, DenyUpload_FeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  EXPECT_TRUE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest, DenyUpload_FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kAutofillUpstream);
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest, DenyUpload_AuthError) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  sync_service_.SetAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest, DenyUpload_SyncDoesNotHaveWalletDataType) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  sync_service_.SetActiveDataTypes(syncer::ModelTypeSet());
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest,
       DenyUpload_FullSyncDoesNotHaveAutofillProfileActiveDataType) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  sync_service_.SetActiveDataTypes(
      syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest,
       DenyUpload_SyncServiceUsingSecondaryPassphrase) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  sync_service_.SetIsUsingSecondaryPassphrase(true);
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest,
       DenyUpload_AutofillWalletImportEnabledPrefIsDisabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  prefs::SetPaymentsIntegrationEnabled(&pref_service_, false);
  EXPECT_FALSE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest, DenyUpload_EmptyUserEmail) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  EXPECT_FALSE(IsCreditCardUploadEnabled(""));
}

TEST_F(AutofillExperimentsTest, AllowUpload_TransportModeOnly) {
  scoped_feature_list_.InitWithFeatures(
      /*enable_features=*/{features::kAutofillUpstream,
                           features::kAutofillEnableAccountWalletStorage},
      /*disable_features=*/{});
  // When we have no primary account, Sync will start in Transport-only mode
  // (if allowed).
  sync_service_.SetIsAuthenticatedAccountPrimary(false);

  EXPECT_TRUE(IsCreditCardUploadEnabled("john.smith@gmail.com"));
}

TEST_F(AutofillExperimentsTest,
       AllowUpload_TransportSyncDoesNotHaveAutofillProfileActiveDataType) {
  scoped_feature_list_.InitWithFeatures(
      /*enable_features=*/{features::kAutofillUpstream,
                           features::kAutofillEnableAccountWalletStorage},
      /*disable_features=*/{});
  // When we have no primary account, Sync will start in Transport-only mode
  // (if allowed).
  sync_service_.SetIsAuthenticatedAccountPrimary(false);

  // Update the active types to only include Wallet. This disables all other
  // types, including profiles.
  sync_service_.SetActiveDataTypes(
      syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));

  EXPECT_TRUE(IsCreditCardUploadEnabled());
}

TEST_F(AutofillExperimentsTest, AllowUpload_UserEmailWithGoogleDomain) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  EXPECT_TRUE(IsCreditCardUploadEnabled("john.smith@gmail.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("googler@google.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("old.school@googlemail.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("code.committer@chromium.org"));
}

TEST_F(AutofillExperimentsTest, DenyUpload_UserEmailWithNonGoogleDomain) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  EXPECT_FALSE(IsCreditCardUploadEnabled("cool.user@hotmail.com"));
  EXPECT_FALSE(IsCreditCardUploadEnabled("john.smith@johnsmith.com"));
  EXPECT_FALSE(IsCreditCardUploadEnabled("fake.googler@google.net"));
  EXPECT_FALSE(IsCreditCardUploadEnabled("fake.committer@chromium.com"));
}

TEST_F(AutofillExperimentsTest,
       AllowUpload_UserEmailWithNonGoogleDomainIfExperimentEnabled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillUpstream,
       features::kAutofillUpstreamAllowAllEmailDomains},
      {});
  EXPECT_TRUE(IsCreditCardUploadEnabled("cool.user@hotmail.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("john.smith@johnsmith.com"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("fake.googler@google.net"));
  EXPECT_TRUE(IsCreditCardUploadEnabled("fake.committer@chromium.com"));
}

}  // namespace autofill
