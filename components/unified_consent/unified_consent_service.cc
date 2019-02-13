// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/pref_names.h"

namespace unified_consent {

UnifiedConsentService::UnifiedConsentService(
    PrefService* pref_service,
    identity::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service) {
  DCHECK(pref_service_);
  DCHECK(identity_manager_);
  DCHECK(sync_service_);

  if (GetMigrationState() == MigrationState::kNotInitialized)
    MigrateProfileToUnifiedConsent();

  identity_manager_->AddObserver(this);
  sync_service_->AddObserver(this);
}

UnifiedConsentService::~UnifiedConsentService() {}

// static
void UnifiedConsentService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                                false);
  registry->RegisterIntegerPref(
      prefs::kUnifiedConsentMigrationState,
      static_cast<int>(MigrationState::kNotInitialized));
}

// static
void UnifiedConsentService::RollbackIfNeeded(
    PrefService* user_pref_service,
    syncer::SyncService* sync_service) {
  DCHECK(user_pref_service);

  if (user_pref_service->GetInteger(prefs::kUnifiedConsentMigrationState) ==
      static_cast<int>(MigrationState::kNotInitialized)) {
    // If there was no migration yet, nothing has to be rolled back.
    return;
  }

  // Clear all unified consent prefs.
  user_pref_service->ClearPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
  user_pref_service->ClearPref(prefs::kUnifiedConsentMigrationState);
}

void UnifiedConsentService::SetUrlKeyedAnonymizedDataCollectionEnabled(
    bool enabled) {
  if (GetMigrationState() != MigrationState::kCompleted)
    SetMigrationState(MigrationState::kCompleted);

  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            enabled);
}

void UnifiedConsentService::Shutdown() {
  identity_manager_->RemoveObserver(this);
  sync_service_->RemoveObserver(this);
}

void UnifiedConsentService::OnPrimaryAccountCleared(
    const CoreAccountInfo& account_info) {
  // By design, clearing the primary account disables URL-keyed data collection.
  SetUrlKeyedAnonymizedDataCollectionEnabled(false);
}

void UnifiedConsentService::OnStateChanged(syncer::SyncService* sync) {
  if (!sync_service_->CanSyncFeatureStart() ||
      !sync_service_->IsEngineInitialized()) {
    return;
  }

  if (GetMigrationState() == MigrationState::kInProgressWaitForSyncInit)
    UpdateSettingsForMigration();
}

MigrationState UnifiedConsentService::GetMigrationState() {
  int migration_state_int =
      pref_service_->GetInteger(prefs::kUnifiedConsentMigrationState);
  DCHECK_LE(static_cast<int>(MigrationState::kNotInitialized),
            migration_state_int);
  DCHECK_GE(static_cast<int>(MigrationState::kCompleted), migration_state_int);
  return static_cast<MigrationState>(migration_state_int);
}

void UnifiedConsentService::SetMigrationState(MigrationState migration_state) {
  pref_service_->SetInteger(prefs::kUnifiedConsentMigrationState,
                            static_cast<int>(migration_state));
}

void UnifiedConsentService::MigrateProfileToUnifiedConsent() {
  DCHECK_EQ(GetMigrationState(), MigrationState::kNotInitialized);

  if (!identity_manager_->HasPrimaryAccount()) {
    SetMigrationState(MigrationState::kCompleted);
    return;
  }

  UpdateSettingsForMigration();
}

void UnifiedConsentService::UpdateSettingsForMigration() {
  if (!sync_service_->IsEngineInitialized()) {
    SetMigrationState(MigrationState::kInProgressWaitForSyncInit);
    return;
  }

  // Set URL-keyed anonymized metrics to the state it had before unified
  // consent.
  bool url_keyed_metrics_enabled =
      sync_service_->IsSyncFeatureEnabled() &&
      sync_service_->GetUserSettings()->GetChosenDataTypes().Has(
          syncer::TYPED_URLS) &&
      !sync_service_->GetUserSettings()->IsUsingSecondaryPassphrase();
  SetUrlKeyedAnonymizedDataCollectionEnabled(url_keyed_metrics_enabled);
}

}  //  namespace unified_consent
