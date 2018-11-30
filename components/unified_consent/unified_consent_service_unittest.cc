// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include <map>
#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service_client.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {
namespace {

class TestSyncService : public syncer::TestSyncService {
 public:
  TestSyncService() = default;

  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observer_ = observer;
  }

  void FireStateChanged() {
    if (observer_)
      observer_->OnStateChanged(this);
  }

 private:
  syncer::SyncServiceObserver* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestSyncService);
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

  static void ClearServiceStates() {
    service_enabled_.clear();
    is_not_supported_.clear();
  }

 private:
  // Service states are shared between multiple instances of this class.
  static std::map<Service, bool> service_enabled_;
  static std::map<Service, bool> is_not_supported_;

  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeUnifiedConsentServiceClient);
};

std::map<Service, bool> FakeUnifiedConsentServiceClient::service_enabled_;
std::map<Service, bool> FakeUnifiedConsentServiceClient::is_not_supported_;

}  // namespace

class UnifiedConsentServiceTest : public testing::Test {
 public:
  UnifiedConsentServiceTest() {
    pref_service_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillWalletImportEnabled, false);
    UnifiedConsentService::RegisterPrefs(pref_service_.registry());
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterBooleanPref(kSpellCheckDummyEnabled,
                                                  false);

    FakeUnifiedConsentServiceClient::ClearServiceStates();
    service_client_ =
        std::make_unique<FakeUnifiedConsentServiceClient>(&pref_service_);
  }

  ~UnifiedConsentServiceTest() override {
    if (consent_service_)
      consent_service_->Shutdown();
  }

  void CreateConsentService(bool client_services_on_by_default = false) {
    if (!scoped_unified_consent_) {
      SetUnifiedConsentFeatureState(
          unified_consent::UnifiedConsentFeatureState::kEnabled);
    }

    auto client =
        std::make_unique<FakeUnifiedConsentServiceClient>(&pref_service_);
    if (client_services_on_by_default) {
      for (int i = 0; i <= static_cast<int>(Service::kLast); ++i) {
        Service service = static_cast<Service>(i);
        client->SetServiceEnabled(service, true);
      }
      pref_service_.SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                               true);
    }
    consent_service_ = std::make_unique<UnifiedConsentService>(
        std::move(client), &pref_service_,
        identity_test_environment_.identity_manager(), &sync_service_);

    sync_service_.FireStateChanged();
    // Run until idle so the migration can finish.
    base::RunLoop().RunUntilIdle();
  }

  void SetUnifiedConsentFeatureState(
      unified_consent::UnifiedConsentFeatureState feature_state) {
    // First reset |scoped_unified_consent_| to nullptr in case it was set
    // before and then initialize it with the new value. This makes sure that
    // the old scoped object is deleted before the new one is created.
    scoped_unified_consent_.reset();
    scoped_unified_consent_.reset(
        new unified_consent::ScopedUnifiedConsent(feature_state));
  }

  bool AreAllGoogleServicesEnabled() {
    for (int i = 0; i <= static_cast<int>(Service::kLast); ++i) {
      Service service = static_cast<Service>(i);
      if (service_client_->GetServiceState(service) == ServiceState::kDisabled)
        return false;
    }
    if (!pref_service_.GetBoolean(
            prefs::kUrlKeyedAnonymizedDataCollectionEnabled))
      return false;

    return true;
  }

  unified_consent::MigrationState GetMigrationState() {
    int migration_state_int =
        pref_service_.GetInteger(prefs::kUnifiedConsentMigrationState);
    return static_cast<unified_consent::MigrationState>(migration_state_int);
  }

 protected:
  base::MessageLoop message_loop_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  identity::IdentityTestEnvironment identity_test_environment_;
  TestSyncService sync_service_;
  std::unique_ptr<UnifiedConsentService> consent_service_;
  std::unique_ptr<FakeUnifiedConsentServiceClient> service_client_;

  std::unique_ptr<ScopedUnifiedConsent> scoped_unified_consent_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedConsentServiceTest);
};

TEST_F(UnifiedConsentServiceTest, DefaultValuesWhenSignedOut) {
  CreateConsentService();
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

TEST_F(UnifiedConsentServiceTest, EnableServices) {
  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount");
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_FALSE(AreAllGoogleServicesEnabled());

  // Enable services and check expectations.
  consent_service_->EnableGoogleServices();
  EXPECT_TRUE(AreAllGoogleServicesEnabled());
}

TEST_F(UnifiedConsentServiceTest, EnableServices_WithUnsupportedService) {
  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount");
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  service_client_->SetServiceNotSupported(Service::kSpellCheck);
  EXPECT_EQ(service_client_->GetServiceState(Service::kSpellCheck),
            ServiceState::kNotSupported);
  EXPECT_FALSE(AreAllGoogleServicesEnabled());

  // Enable services and check expectations.
  consent_service_->EnableGoogleServices();
  EXPECT_TRUE(AreAllGoogleServicesEnabled());
}

TEST_F(UnifiedConsentServiceTest, Migration_UpdateSettings) {
  // Create user that syncs history and has no custom passphrase.
  identity_test_environment_.SetPrimaryAccount("testaccount");
  sync_service_.GetUserSettings()->SetChosenDataTypes(
      false, syncer::ModelTypeSet(syncer::TYPED_URLS));
  EXPECT_TRUE(sync_service_.IsSyncFeatureActive());
  // Url keyed data collection is off before the migration.
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  CreateConsentService();
  EXPECT_EQ(GetMigrationState(), unified_consent::MigrationState::kCompleted);
  // During the migration Url keyed data collection is enabled.
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

#if !defined(OS_CHROMEOS)
TEST_F(UnifiedConsentServiceTest, ClearPrimaryAccountDisablesSomeServices) {
  base::HistogramTester histogram_tester;

  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount");

  // Precondition: Enable unified consent.
  consent_service_->EnableGoogleServices();
  EXPECT_TRUE(AreAllGoogleServicesEnabled());

  // Clearing primary account revokes unfied consent and a couple of other
  // non-personalized services.
  identity_test_environment_.ClearPrimaryAccount();
  EXPECT_FALSE(AreAllGoogleServicesEnabled());
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_EQ(service_client_->GetServiceState(Service::kSpellCheck),
            ServiceState::kDisabled);
  EXPECT_EQ(
      service_client_->GetServiceState(Service::kSafeBrowsingExtendedReporting),
      ServiceState::kDisabled);
  EXPECT_EQ(service_client_->GetServiceState(Service::kContextualSearch),
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

TEST_F(UnifiedConsentServiceTest, Migration_NotSignedIn) {
  base::HistogramTester histogram_tester;

  CreateConsentService();
  // The user is signed out, so the migration is completed after the
  // creation of the consent service.
  EXPECT_EQ(GetMigrationState(), unified_consent::MigrationState::kCompleted);
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(UnifiedConsentServiceTest, Rollback_UserOptedIntoUnifiedConsent) {
  identity_test_environment_.SetPrimaryAccount("testaccount");

  // Migrate and opt into unified consent.
  CreateConsentService();
  consent_service_->EnableGoogleServices();
  // Check expectations after opt-in.
  EXPECT_TRUE(AreAllGoogleServicesEnabled());
  EXPECT_EQ(unified_consent::MigrationState::kCompleted, GetMigrationState());
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kAllUnifiedConsentServicesWereEnabled));

  consent_service_->Shutdown();
  consent_service_.reset();
  SetUnifiedConsentFeatureState(UnifiedConsentFeatureState::kDisabled);

  // Rollback
  UnifiedConsentService::RollbackIfNeeded(&pref_service_, &sync_service_,
                                          service_client_.get());

  // Unified consent prefs should be cleared.
  EXPECT_EQ(unified_consent::MigrationState::kNotInitialized,
            GetMigrationState());
  // Off-by-default services should be turned off.
  EXPECT_NE(ServiceState::kEnabled,
            service_client_->GetServiceState(
                Service::kSafeBrowsingExtendedReporting));
  EXPECT_NE(ServiceState::kEnabled,
            service_client_->GetServiceState(Service::kSpellCheck));
  EXPECT_NE(ServiceState::kEnabled,
            service_client_->GetServiceState(Service::kContextualSearch));
}

}  // namespace unified_consent
