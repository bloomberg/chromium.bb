// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include <map>
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

const char kSpellCheckDummyEnabled[] = "spell_check_dummy.enabled";

class FakeUnifiedConsentServiceClient : public UnifiedConsentServiceClient {
 public:
  FakeUnifiedConsentServiceClient(PrefService* pref_service)
      : pref_service_(pref_service) {
    // When the |kSpellCheckDummyEnabled| pref is changed, all observers should
    // be fired.
    ObserveServicePrefChange(Service::kSpellCheck, kSpellCheckDummyEnabled,
                             pref_service);
  }
  ~FakeUnifiedConsentServiceClient() override = default;

  // UnifiedConsentServiceClient:
  ServiceState GetServiceState(Service service) override {
    if (is_not_supported_[service])
      return ServiceState::kNotSupported;
    bool enabled;
    // Special treatment for spell check.
    if (service == Service::kSpellCheck) {
      enabled = pref_service_->GetBoolean(kSpellCheckDummyEnabled);
    } else {
      enabled = service_enabled_[service];
    }
    return enabled ? ServiceState::kEnabled : ServiceState::kDisabled;
  }
  void SetServiceEnabled(Service service, bool enabled) override {
    if (is_not_supported_[service])
      return;
    // Special treatment for spell check.
    if (service == Service::kSpellCheck) {
      pref_service_->SetBoolean(kSpellCheckDummyEnabled, enabled);
      return;
    }
    bool should_notify_observers = service_enabled_[service] != enabled;
    service_enabled_[service] = enabled;
    if (should_notify_observers)
      FireOnServiceStateChanged(service);
  }

  void SetServiceNotSupported(Service service) {
    is_not_supported_[service] = true;
  }

 private:
  std::map<Service, bool> service_enabled_;
  std::map<Service, bool> is_not_supported_;

  PrefService* pref_service_;
};

}  // namespace

class UnifiedConsentServiceTest : public testing::Test {
 public:
  // testing::Test:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillWalletImportEnabled, false);
    UnifiedConsentService::RegisterPrefs(pref_service_.registry());
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterBooleanPref(kSpellCheckDummyEnabled,
                                                  false);
  }

  void TearDown() override { consent_service_->Shutdown(); }

  void CreateConsentService() {
    consent_service_ = std::make_unique<UnifiedConsentService>(
        std::make_unique<FakeUnifiedConsentServiceClient>(&pref_service_),
        &pref_service_, identity_test_environment_.identity_manager(),
        &sync_service_);
    service_client_ = (FakeUnifiedConsentServiceClient*)
                          consent_service_->service_client_.get();
  }

  // Returns true if all supported non-personalized services are enabled.
  bool AreAllNonPersonalizedServicesEnabled() {
    for (int service = 0; service <= static_cast<int>(Service::kLast);
         ++service) {
      if (service_client_->GetServiceState(static_cast<Service>(service)) ==
          ServiceState::kDisabled) {
        return false;
      }
    }
    if (!pref_service_.GetBoolean(
            prefs::kUrlKeyedAnonymizedDataCollectionEnabled))
      return false;

    return true;
  }

 protected:
  base::MessageLoop message_loop_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  identity::IdentityTestEnvironment identity_test_environment_;
  TestSyncService sync_service_;
  std::unique_ptr<UnifiedConsentService> consent_service_;
  FakeUnifiedConsentServiceClient* service_client_ = nullptr;
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
  EXPECT_FALSE(AreAllNonPersonalizedServicesEnabled());

  // Enable Unified Consent enables all non-personaized features
  pref_service_.SetBoolean(prefs::kUnifiedConsentGiven, true);
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_TRUE(AreAllNonPersonalizedServicesEnabled());

  // Disable unified consent does not disable any of the non-personalized
  // features.
  pref_service_.SetBoolean(prefs::kUnifiedConsentGiven, false);
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_TRUE(AreAllNonPersonalizedServicesEnabled());
}

TEST_F(UnifiedConsentServiceTest, EnableUnfiedConsent_WithUnsupportedService) {
  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount");
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_FALSE(AreAllNonPersonalizedServicesEnabled());
  service_client_->SetServiceNotSupported(Service::kSpellCheck);
  EXPECT_EQ(service_client_->GetServiceState(Service::kSpellCheck),
            ServiceState::kNotSupported);
  EXPECT_FALSE(AreAllNonPersonalizedServicesEnabled());

  // Enable Unified Consent enables all supported non-personalized features
  pref_service_.SetBoolean(prefs::kUnifiedConsentGiven, true);
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_TRUE(AreAllNonPersonalizedServicesEnabled());

  // Disable unified consent does not disable any of the supported
  // non-personalized features.
  pref_service_.SetBoolean(prefs::kUnifiedConsentGiven, false);
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_TRUE(AreAllNonPersonalizedServicesEnabled());
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

// Test whether unified consent is disabled when any of its dependent services
// gets disabled.
TEST_F(UnifiedConsentServiceTest, DisableUnfiedConsentWhenServiceIsDisabled) {
  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount");
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_FALSE(AreAllNonPersonalizedServicesEnabled());

  // Enable Unified Consent enables all supported non-personalized features
  pref_service_.SetBoolean(prefs::kUnifiedConsentGiven, true);
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_TRUE(AreAllNonPersonalizedServicesEnabled());

  // Disabling child service disables unified consent.
  pref_service_.SetBoolean(kSpellCheckDummyEnabled, false);
  EXPECT_FALSE(AreAllNonPersonalizedServicesEnabled());
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
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
  EXPECT_TRUE(AreAllNonPersonalizedServicesEnabled());

  // Clearing primary account revokes unfied consent and a couple of other
  // non-personalized services.
  identity_test_environment_.ClearPrimaryAccount();
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUnifiedConsentGiven));
  EXPECT_FALSE(AreAllNonPersonalizedServicesEnabled());
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_EQ(service_client_->GetServiceState(Service::kSpellCheck),
            ServiceState::kDisabled);
  EXPECT_EQ(
      service_client_->GetServiceState(Service::kSafeBrowsingExtendedReporting),
      ServiceState::kDisabled);

  // Consent is not revoked for the following services.
  EXPECT_EQ(service_client_->GetServiceState(Service::kAlternateErrorPages),
            ServiceState::kEnabled);
  EXPECT_EQ(service_client_->GetServiceState(Service::kMetricsReporting),
            ServiceState::kEnabled);
  EXPECT_EQ(service_client_->GetServiceState(Service::kNetworkPrediction),
            ServiceState::kEnabled);
  EXPECT_EQ(service_client_->GetServiceState(Service::kSearchSuggest),
            ServiceState::kEnabled);
  EXPECT_EQ(service_client_->GetServiceState(Service::kSafeBrowsing),
            ServiceState::kEnabled);
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace unified_consent
