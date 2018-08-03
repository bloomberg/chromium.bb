// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/unified_consent/unified_consent_service_client.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefChangeRegistrar;
class PrefService;

namespace syncer {
class SyncService;
}

namespace unified_consent {

using Service = UnifiedConsentServiceClient::Service;
using ServiceState = UnifiedConsentServiceClient::ServiceState;

enum class MigrationState : int {
  NOT_INITIALIZED = 0,
  IN_PROGRESS_SHOULD_SHOW_CONSENT_BUMP = 1,
  // Reserve space for other IN_PROGRESS_* entries to be added here.
  COMPLETED = 10,
};

// A browser-context keyed service that is used to manage the user consent
// when UnifiedConsent feature is enabled.
class UnifiedConsentService : public KeyedService,
                              public UnifiedConsentServiceClient::Observer,
                              public identity::IdentityManager::Observer,
                              public syncer::SyncServiceObserver {
 public:
  UnifiedConsentService(
      std::unique_ptr<UnifiedConsentServiceClient> service_client,
      PrefService* pref_service,
      identity::IdentityManager* identity_manager,
      syncer::SyncService* sync_service);
  ~UnifiedConsentService() override;

  // Register the prefs used by this UnifiedConsentService.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // This updates the consent pref and if |unified_consent_given| is true, all
  // unified consent services are enabled.
  void SetUnifiedConsentGiven(bool unified_consent_given);
  bool IsUnifiedConsentGiven();

  // Helper function for the consent bump. The consent bump has to
  // be shown depending on the migration state.
  MigrationState GetMigrationState();
  // Returns true if the consent bump needs to be shown to the user as part
  // of the migration of the Chrome profile to unified consent.
  bool ShouldShowConsentBump();
  // Finishes the migration to unified consent. All future calls to
  // |GetMigrationState| are guranteed to return |MIGRATION_COMPLETED|.
  void MarkMigrationComplete();

  // KeyedService:
  void Shutdown() override;

  // UnifiedConsentServiceClient::Observer:
  void OnServiceStateChanged(Service service) override;

  // IdentityManager::Observer:
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;

 private:
  friend class UnifiedConsentServiceTest;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // Called when |prefs::kUnifiedConsentGiven| pref value changes.
  // When set to true, it enables syncing of all data types and it enables all
  // non-personalized services. Otherwise it does nothing.
  void OnUnifiedConsentGivenPrefChanged();

  // Enables/disables syncing everything if the sync engine is initialized.
  void SetSyncEverythingIfPossible(bool sync_everything);

  // Called when the unified consent service is created to resolve
  // inconsistencies with sync-related prefs.
  void MigrateProfileToUnifiedConsent();

  // Returns true if all non-personalized services are enabled.
  bool AreAllNonPersonalizedServicesEnabled();

  std::unique_ptr<UnifiedConsentServiceClient> service_client_;
  PrefService* pref_service_;
  identity::IdentityManager* identity_manager_;
  syncer::SyncService* sync_service_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedConsentService);
};

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_
