// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service_client.h"

namespace unified_consent {

UnifiedConsentService::UnifiedConsentService(
    UnifiedConsentServiceClient* service_client,
    PrefService* pref_service,
    identity::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : service_client_(service_client),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service) {
  DCHECK(pref_service_);
  DCHECK(identity_manager_);
  DCHECK(sync_service_);
  identity_manager_->AddObserver(this);

  if (GetMigrationState() == MigrationState::NOT_INITIALIZED)
    MigrateProfileToUnifiedConsent();

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUnifiedConsentGiven,
      base::BindRepeating(
          &UnifiedConsentService::OnUnifiedConsentGivenPrefChanged,
          base::Unretained(this)));
}

UnifiedConsentService::~UnifiedConsentService() {}

void UnifiedConsentService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kUnifiedConsentGiven, false);
  registry->RegisterIntegerPref(
      prefs::kUnifiedConsentMigrationState,
      static_cast<int>(MigrationState::NOT_INITIALIZED));
}

void UnifiedConsentService::SetUnifiedConsentGiven(bool unified_consent_given) {
  pref_service_->SetBoolean(prefs::kUnifiedConsentGiven, unified_consent_given);
}

bool UnifiedConsentService::IsUnifiedConsentGiven() {
  return pref_service_->GetBoolean(prefs::kUnifiedConsentGiven);
}

MigrationState UnifiedConsentService::GetMigrationState() {
  int migration_state_int =
      pref_service_->GetInteger(prefs::kUnifiedConsentMigrationState);
  DCHECK_LE(static_cast<int>(MigrationState::NOT_INITIALIZED),
            migration_state_int);
  DCHECK_GE(static_cast<int>(MigrationState::COMPLETED), migration_state_int);
  return static_cast<MigrationState>(migration_state_int);
}

bool UnifiedConsentService::ShouldShowConsentBump() {
  return GetMigrationState() ==
         MigrationState::IN_PROGRESS_SHOULD_SHOW_CONSENT_BUMP;
}

void UnifiedConsentService::MarkMigrationComplete() {
  pref_service_->SetInteger(prefs::kUnifiedConsentMigrationState,
                            static_cast<int>(MigrationState::COMPLETED));
}

void UnifiedConsentService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

void UnifiedConsentService::OnPrimaryAccountCleared(
    const AccountInfo& account_info) {
  // By design, signing out of Chrome automatically disables the user consent
  // for making search and browsing better.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            false);
  // When signing out, the unfied consent is revoked.
  pref_service_->SetBoolean(prefs::kUnifiedConsentGiven, false);

  switch (GetMigrationState()) {
    case MigrationState::NOT_INITIALIZED:
      NOTREACHED();
      break;
    case MigrationState::IN_PROGRESS_SHOULD_SHOW_CONSENT_BUMP:
      // Only users that were signed in and have opted into sync before unified
      // consent are eligible to see the unified consent bump. Since the user
      // signs out of Chrome, mark the migration to unified consent as complete.
      MarkMigrationComplete();
      break;
    case MigrationState::COMPLETED:
      break;
  }
}

void UnifiedConsentService::OnUnifiedConsentGivenPrefChanged() {
  bool enabled = pref_service_->GetBoolean(prefs::kUnifiedConsentGiven);

  if (!enabled) {
    if (identity_manager_->HasPrimaryAccount()) {
      // KeepEverythingSynced is set to false, so the user can select individual
      // sync data types.
      sync_service_->OnUserChoseDatatypes(false, syncer::UserSelectableTypes());
    }
    return;
  }

  DCHECK(sync_service_->IsSyncAllowed());
  DCHECK(identity_manager_->HasPrimaryAccount());
  DCHECK_LT(MigrationState::NOT_INITIALIZED, GetMigrationState());

  // If the user opts into unified consent throught settings, the consent bump
  // doesn't need to be shown. Therefore mark the migration as complete.
  if (GetMigrationState() != MigrationState::COMPLETED)
    MarkMigrationComplete();

  // Enable all sync data types.
  pref_service_->SetBoolean(autofill::prefs::kAutofillWalletImportEnabled,
                            true);
  sync_service_->OnUserChoseDatatypes(true, syncer::UserSelectableTypes());

  // Enable all non-personalized services.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            true);

  // Inform client to enable non-personalized services.
  service_client_->SetAlternateErrorPagesEnabled(true);
  service_client_->SetMetricsReportingEnabled(true);
  service_client_->SetSafeBrowsingExtendedReportingEnabled(true);
  service_client_->SetSearchSuggestEnabled(true);
  service_client_->SetSafeBrowsingEnabled(true);
  service_client_->SetSafeBrowsingExtendedReportingEnabled(true);
  service_client_->SetNetworkPredictionEnabled(true);
}

void UnifiedConsentService::MigrateProfileToUnifiedConsent() {
  DCHECK_EQ(GetMigrationState(), MigrationState::NOT_INITIALIZED);
  DCHECK(!IsUnifiedConsentGiven());

  syncer::SyncPrefs sync_prefs(pref_service_);
  if (identity_manager_->HasPrimaryAccount() &&
      sync_prefs.HasKeepEverythingSynced()) {
    // Set keep-everything-synced pref to false to match |kUnifiedConsentGiven|.
    sync_prefs.SetKeepEverythingSynced(false);
    // When the user was syncing everything, the consent bump should be shown
    // when this is possible.
    pref_service_->SetInteger(
        prefs::kUnifiedConsentMigrationState,
        static_cast<int>(MigrationState::IN_PROGRESS_SHOULD_SHOW_CONSENT_BUMP));
    return;
  }

  // When the user isn't signed in or doesn't sync everything, nothing has to be
  // done. Mark migration as complete.
  MarkMigrationComplete();
}

}  //  namespace unified_consent
