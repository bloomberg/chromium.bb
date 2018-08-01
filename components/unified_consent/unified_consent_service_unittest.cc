// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service_client.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {
namespace {

class TestSyncService : public syncer::FakeSyncService {
 public:
  int GetDisableReasons() const override { return DISABLE_REASON_NONE; }
  State GetState() const override { return state_; }
  bool IsFirstSetupComplete() const override { return true; }
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observer_ = observer;
  }
  void OnUserChoseDatatypes(bool sync_everything,
                            syncer::ModelTypeSet chosen_types) override {
    is_syncing_everything_ = sync_everything;
  }

  void SetState(State state) { state_ = state; }
  void FireStateChanged() {
    if (observer_)
      observer_->OnStateChanged(this);
  }
  // This is a helper if the value is set through |OnUserChoseDatatypes|, which
  // is not implemented in |FakeSyncService|. Usually
  // |sync_prefs_.HasKeepEverythingSynced()| is used.
  bool IsSyncingEverything() { return is_syncing_everything_; }

 private:
  syncer::SyncServiceObserver* observer_ = nullptr;
  State state_ = State::ACTIVE;
  bool is_syncing_everything_ = false;
};

class UnifiedConsentServiceTest : public testing::Test {
 public:
  class FakeUnifiedConsentServiceClient : public UnifiedConsentServiceClient {
   public:
    FakeUnifiedConsentServiceClient(UnifiedConsentServiceTest* test)
        : test_(test) {}
    // UnifiedConsentServiceClient:
    void SetAlternateErrorPagesEnabled(bool enabled) override {
      test_->alternate_error_pages_enabled_ = enabled;
    }
    void SetMetricsReportingEnabled(bool enabled) override {
      test_->metrics_reporting_enabled_ = enabled;
    }
    void SetNetworkPredictionEnabled(bool enabled) override {
      test_->network_predictions_enabled_ = enabled;
    }
    void SetSafeBrowsingEnabled(bool enabled) override {
      test_->safe_browsing_enabled_ = enabled;
    }
    void SetSafeBrowsingExtendedReportingEnabled(bool enabled) override {
      test_->safe_browsing_extended_reporting_enabled_ = enabled;
    }
    void SetSearchSuggestEnabled(bool enabled) override {
      test_->search_suggest_enabled_ = enabled;
    }
    void SetSpellCheckEnabled(bool enabled) override {
      test_->spell_check_enabled_ = enabled;
    }

   private:
    UnifiedConsentServiceTest* test_;
  };

  // testing::Test:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillWalletImportEnabled, false);
    UnifiedConsentService::RegisterPrefs(pref_service_.registry());
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

  void TearDown() override { consent_service_->Shutdown(); }

  void CreateConsentService() {
    consent_service_ = std::make_unique<UnifiedConsentService>(
        std::make_unique<FakeUnifiedConsentServiceClient>(this), &pref_service_,
        identity_test_environment_.identity_manager(), &sync_service_);
  }

 protected:
  base::MessageLoop message_loop_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  identity::IdentityTestEnvironment identity_test_environment_;
  TestSyncService sync_service_;
  std::unique_ptr<UnifiedConsentService> consent_service_;
  bool alternate_error_pages_enabled_ = false;
  bool metrics_reporting_enabled_ = false;
  bool network_predictions_enabled_ = false;
  bool safe_browsing_enabled_ = false;
  bool safe_browsing_extended_reporting_enabled_ = false;
  bool search_suggest_enabled_ = false;
  bool spell_check_enabled_ = false;
};

TEST_F(UnifiedConsentServiceTest, DefaultValuesWhenSignedOut) {
  CreateConsentService();
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

TEST_F(UnifiedConsentServiceTest, EnableUnfiedConsent) {
  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount");
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Enable Unified Consent enables all non-personaized features
  pref_service_.SetBoolean(prefs::kUnifiedConsentGiven, true);
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(alternate_error_pages_enabled_);
  EXPECT_TRUE(metrics_reporting_enabled_);
  EXPECT_TRUE(network_predictions_enabled_);
  EXPECT_TRUE(safe_browsing_enabled_);
  EXPECT_TRUE(safe_browsing_extended_reporting_enabled_);
  EXPECT_TRUE(search_suggest_enabled_);
  EXPECT_TRUE(spell_check_enabled_);

  // Disable unified consent does not disable any of the non-personalized
  // features.
  pref_service_.SetBoolean(prefs::kUnifiedConsentGiven, false);
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(alternate_error_pages_enabled_);
  EXPECT_TRUE(metrics_reporting_enabled_);
  EXPECT_TRUE(network_predictions_enabled_);
  EXPECT_TRUE(safe_browsing_enabled_);
  EXPECT_TRUE(safe_browsing_extended_reporting_enabled_);
  EXPECT_TRUE(search_suggest_enabled_);
  EXPECT_TRUE(spell_check_enabled_);
}

TEST_F(UnifiedConsentServiceTest, EnableUnfiedConsent_SyncNotActive) {
  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount");
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_FALSE(sync_service_.IsSyncingEverything());
  syncer::SyncPrefs sync_prefs(&pref_service_);
  sync_prefs.SetKeepEverythingSynced(false);
  EXPECT_FALSE(sync_prefs.HasKeepEverythingSynced());
  EXPECT_FALSE(consent_service_->IsUnifiedConsentGiven());

  // Make sure sync is not active.
  sync_service_.SetState(syncer::SyncService::State::INITIALIZING);
  EXPECT_FALSE(sync_service_.IsEngineInitialized());
  EXPECT_NE(sync_service_.GetState(), syncer::SyncService::State::ACTIVE);

  // Opt into unified consent.
  consent_service_->SetUnifiedConsentGiven(true);
  EXPECT_TRUE(consent_service_->IsUnifiedConsentGiven());

  // Couldn't sync everything because sync is not active.
  EXPECT_FALSE(sync_service_.IsSyncingEverything());

  // Initalize sync engine and therefore activate sync.
  sync_service_.SetState(syncer::SyncService::State::ACTIVE);
  sync_service_.FireStateChanged();

  // UnifiedConsentService starts syncing everything.
  EXPECT_TRUE(sync_service_.IsSyncingEverything());
}

#if !defined(OS_CHROMEOS)
TEST_F(UnifiedConsentServiceTest, Migration_SyncingEverything) {
  base::HistogramTester histogram_tester;

  // Create inconsistent state.
  identity_test_environment_.SetPrimaryAccount("testaccount");
  syncer::SyncPrefs sync_prefs(&pref_service_);
  sync_prefs.SetKeepEverythingSynced(true);
  EXPECT_TRUE(sync_prefs.HasKeepEverythingSynced());
  sync_service_.OnUserChoseDatatypes(true, {});
  EXPECT_TRUE(sync_service_.IsSyncingEverything());
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));

  CreateConsentService();
  // After the creation of the consent service, inconsistencies are resolved and
  // the migration state should be in-progress (i.e. the consent bump should be
  // shown).
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_FALSE(sync_service_.IsSyncingEverything());
  EXPECT_EQ(
      consent_service_->GetMigrationState(),
      unified_consent::MigrationState::IN_PROGRESS_SHOULD_SHOW_CONSENT_BUMP);
  // A metric should be recorded that the user was syncing everything.
  histogram_tester.ExpectBucketCount(
      "UnifiedConsent.Migration.SyncEverythingWasOn", true, 1);

  // When the user signs out, the migration state changes to completed.
  identity_test_environment_.ClearPrimaryAccount();
  EXPECT_EQ(consent_service_->GetMigrationState(),
            unified_consent::MigrationState::COMPLETED);
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(UnifiedConsentServiceTest, Migration_NotSyncingEverything) {
  base::HistogramTester histogram_tester;

  identity_test_environment_.SetPrimaryAccount("testaccount");
  syncer::SyncPrefs sync_prefs(&pref_service_);
  sync_prefs.SetKeepEverythingSynced(false);
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_FALSE(sync_prefs.HasKeepEverythingSynced());

  CreateConsentService();
  // Since there were not inconsistencies, the migration is completed after the
  // creation of the consent service.
  EXPECT_EQ(consent_service_->GetMigrationState(),
            unified_consent::MigrationState::COMPLETED);
  // A metric should be recorded that the user wasn't syncing everything.
  histogram_tester.ExpectBucketCount(
      "UnifiedConsent.Migration.SyncEverythingWasOn", false, 1);
}

#if !defined(OS_CHROMEOS)
TEST_F(UnifiedConsentServiceTest, ClearPrimaryAccountDisablesSomeServices) {
  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount");

  // Precondition: Enable unified consent.
  pref_service_.SetBoolean(prefs::kUnifiedConsentGiven, true);
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(alternate_error_pages_enabled_);
  EXPECT_TRUE(metrics_reporting_enabled_);
  EXPECT_TRUE(network_predictions_enabled_);
  EXPECT_TRUE(safe_browsing_enabled_);
  EXPECT_TRUE(safe_browsing_extended_reporting_enabled_);
  EXPECT_TRUE(search_suggest_enabled_);
  EXPECT_TRUE(spell_check_enabled_);

  // Clearing primary account revokes unfied consent and a couple of other
  // non-personalized services.
  identity_test_environment_.ClearPrimaryAccount();
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_FALSE(spell_check_enabled_);
  EXPECT_FALSE(safe_browsing_extended_reporting_enabled_);

  // Consent is not revoked for the following services.
  EXPECT_TRUE(alternate_error_pages_enabled_);
  EXPECT_TRUE(metrics_reporting_enabled_);
  EXPECT_TRUE(network_predictions_enabled_);
  EXPECT_TRUE(safe_browsing_enabled_);
  EXPECT_TRUE(search_suggest_enabled_);
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace
}  // namespace unified_consent
