// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service_client.h"

namespace unified_consent {

UnifiedConsentService::UnifiedConsentService(
    std::unique_ptr<UnifiedConsentServiceClient> service_client,
    PrefService* pref_service,
    identity::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : service_client_(std::move(service_client)),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service) {
  DCHECK(service_client_);
  DCHECK(pref_service_);
  DCHECK(identity_manager_);
  DCHECK(sync_service_);

  if (GetMigrationState() == MigrationState::NOT_INITIALIZED)
    MigrateProfileToUnifiedConsent();

  service_client_->AddObserver(this);
  identity_manager_->AddObserver(this);
  sync_service_->AddObserver(this);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUnifiedConsentGiven,
      base::BindRepeating(
          &UnifiedConsentService::OnUnifiedConsentGivenPrefChanged,
          base::Unretained(this)));

  // If somebody disabled any of the non-personalized services while Chrome
  // wasn't running, disable unified consent.
  if (!AreAllNonPersonalizedServicesEnabled() && IsUnifiedConsentGiven()) {
    SetUnifiedConsentGiven(false);
  }
}

UnifiedConsentService::~UnifiedConsentService() {}

// static
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
  if (base::FeatureList::IsEnabled(unified_consent::kForceUnifiedConsentBump))
    return true;
  return GetMigrationState() ==
         MigrationState::IN_PROGRESS_SHOULD_SHOW_CONSENT_BUMP;
}

void UnifiedConsentService::MarkMigrationComplete(
    ConsentBumpSuppressReason suppress_reason) {
  pref_service_->SetInteger(prefs::kUnifiedConsentMigrationState,
                            static_cast<int>(MigrationState::COMPLETED));
  // Record the suppress reason for the consent bump. After the migration is
  // marked complete, the consent bump should not be shown anymore. Note:
  // |suppress_reason| can be kNone in case the consent bump was actually shown.
  RecordConsentBumpSuppressReason(suppress_reason);
}

void UnifiedConsentService::RecordConsentBumpSuppressReason(
    ConsentBumpSuppressReason suppress_reason) {
  UMA_HISTOGRAM_ENUMERATION("UnifiedConsent.ConsentBump.SuppressReason",
                            suppress_reason);
}

void UnifiedConsentService::Shutdown() {
  service_client_->RemoveObserver(this);
  identity_manager_->RemoveObserver(this);
  sync_service_->RemoveObserver(this);
}

void UnifiedConsentService::OnServiceStateChanged(Service service) {
  // Unified consent is disabled when any of its dependent services gets
  // disabled.
  if (service_client_->GetServiceState(service) == ServiceState::kDisabled)
    SetUnifiedConsentGiven(false);
}

void UnifiedConsentService::OnPrimaryAccountCleared(
    const AccountInfo& account_info) {
  // When signing out, the unfied consent is revoked.
  pref_service_->SetBoolean(prefs::kUnifiedConsentGiven, false);

  // By design, signing out of Chrome automatically disables off-by-default
  // services.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            false);
  service_client_->SetServiceEnabled(Service::kSafeBrowsingExtendedReporting,
                                     false);
  service_client_->SetServiceEnabled(Service::kSpellCheck, false);

  switch (GetMigrationState()) {
    case MigrationState::NOT_INITIALIZED:
      NOTREACHED();
      break;
    case MigrationState::IN_PROGRESS_SHOULD_SHOW_CONSENT_BUMP:
      // Only users that were signed in and have opted into sync before unified
      // consent are eligible to see the unified consent bump. Since the user
      // signs out of Chrome, mark the migration to unified consent as complete.
      MarkMigrationComplete(ConsentBumpSuppressReason::kUserSignedOut);
      break;
    case MigrationState::COMPLETED:
      break;
  }
}

void UnifiedConsentService::OnStateChanged(syncer::SyncService* sync) {
  syncer::SyncPrefs sync_prefs(pref_service_);
  if (IsUnifiedConsentGiven() != sync_prefs.HasKeepEverythingSynced()) {
    // Make sync-everything consistent with the |kUnifiedConsentGiven| pref.
    SetSyncEverythingIfPossible(IsUnifiedConsentGiven());
  }
}

void UnifiedConsentService::OnUnifiedConsentGivenPrefChanged() {
  bool enabled = pref_service_->GetBoolean(prefs::kUnifiedConsentGiven);

  if (!enabled) {
    if (identity_manager_->HasPrimaryAccount()) {
      // Sync-everything is set to false, so the user can select individual
      // sync data types.
      SetSyncEverythingIfPossible(false);
    }
    return;
  }

  DCHECK(!sync_service_->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_PLATFORM_OVERRIDE));
  DCHECK(!sync_service_->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY));
  DCHECK(identity_manager_->HasPrimaryAccount());
  DCHECK_LT(MigrationState::NOT_INITIALIZED, GetMigrationState());

  // If the user opts into unified consent throught settings, the consent bump
  // doesn't need to be shown. Therefore mark the migration as complete.
  if (GetMigrationState() != MigrationState::COMPLETED)
    MarkMigrationComplete(ConsentBumpSuppressReason::kSettingsOptIn);

  // Enable all sync data types if possible, otherwise they will be enabled with
  // |OnStateChanged| once sync is active;
  SetSyncEverythingIfPossible(true);

  // Enable all non-personalized services.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            true);
  // Inform client to enable non-personalized services.
  for (int i = 0; i <= static_cast<int>(Service::kLast); ++i) {
    Service service = static_cast<Service>(i);
    if (service_client_->GetServiceState(service) !=
        ServiceState::kNotSupported) {
      service_client_->SetServiceEnabled(service, true);
    }
  }
}

void UnifiedConsentService::SetSyncEverythingIfPossible(bool sync_everything) {
  syncer::SyncPrefs sync_prefs(pref_service_);
  if (sync_everything == sync_prefs.HasKeepEverythingSynced())
    return;

  if (!sync_service_->IsEngineInitialized())
    return;

  if (sync_everything) {
    autofill::prefs::SetPaymentsIntegrationEnabled(pref_service_, true);
    sync_service_->OnUserChoseDatatypes(sync_everything,
                                        syncer::UserSelectableTypes());
  } else {
    syncer::ModelTypeSet preferred = sync_service_->GetPreferredDataTypes();
    preferred.RetainAll(syncer::UserSelectableTypes());
    sync_service_->OnUserChoseDatatypes(false, preferred);
  }
}

void UnifiedConsentService::MigrateProfileToUnifiedConsent() {
  DCHECK_EQ(GetMigrationState(), MigrationState::NOT_INITIALIZED);
  DCHECK(!IsUnifiedConsentGiven());

  if (!identity_manager_->HasPrimaryAccount()) {
    MarkMigrationComplete(ConsentBumpSuppressReason::kNotSignedIn);
    return;
  }

  bool is_syncing_everything =
      syncer::SyncPrefs(pref_service_).HasKeepEverythingSynced();
  if (!is_syncing_everything) {
    MarkMigrationComplete(ConsentBumpSuppressReason::kSyncEverythingOff);
    return;
  }

  // Set sync-everything to false to match the |kUnifiedConsentGiven| pref.
  // Note: If the sync engine isn't initialized at this point,
  // sync-everything is set to false once the sync engine state changes.
  // Sync-everything can then be set to true again after going through the
  // consent bump and opting into unified consent.
  SetSyncEverythingIfPossible(false);

  if (!AreAllOnByDefaultPrivacySettingsOn()) {
    MarkMigrationComplete(ConsentBumpSuppressReason::kPrivacySettingOff);
    return;
  }

  // When the user was syncing everything, and all on-by-default privacy
  // settings were on, the consent bump should be shown when this is possible.
  pref_service_->SetInteger(
      prefs::kUnifiedConsentMigrationState,
      static_cast<int>(MigrationState::IN_PROGRESS_SHOULD_SHOW_CONSENT_BUMP));
}

bool UnifiedConsentService::AreAllNonPersonalizedServicesEnabled() {
  for (int i = 0; i <= static_cast<int>(Service::kLast); ++i) {
    Service service = static_cast<Service>(i);
    if (service_client_->GetServiceState(service) == ServiceState::kDisabled)
      return false;
  }
  if (!pref_service_->GetBoolean(
          prefs::kUrlKeyedAnonymizedDataCollectionEnabled))
    return false;

  return true;
}

bool UnifiedConsentService::AreAllOnByDefaultPrivacySettingsOn() {
  for (auto service : {Service::kAlternateErrorPages,
                       Service::kMetricsReporting, Service::kNetworkPrediction,
                       Service::kSafeBrowsing, Service::kSearchSuggest}) {
    if (service_client_->GetServiceState(service) == ServiceState::kDisabled)
      return false;
  }
  return true;
}

}  //  namespace unified_consent
