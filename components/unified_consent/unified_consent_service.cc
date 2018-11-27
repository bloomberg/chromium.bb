// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/pref_names.h"

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

  if (GetMigrationState() == MigrationState::kNotInitialized)
    MigrateProfileToUnifiedConsent();

  // Check if this profile is still eligible for the consent bump.
  CheckConsentBumpEligibility();

  identity_manager_->AddObserver(this);
  sync_service_->AddObserver(this);

  RecordSettingsHistogram();
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
  registry->RegisterBooleanPref(prefs::kShouldShowUnifiedConsentBump, false);
  registry->RegisterBooleanPref(prefs::kAllUnifiedConsentServicesWereEnabled,
                                false);
}

// static
void UnifiedConsentService::RollbackIfNeeded(
    PrefService* user_pref_service,
    syncer::SyncService* sync_service,
    UnifiedConsentServiceClient* service_client) {
  DCHECK(user_pref_service);
  DCHECK(service_client);

  if (user_pref_service->GetInteger(prefs::kUnifiedConsentMigrationState) ==
      static_cast<int>(MigrationState::kNotInitialized)) {
    // If there was no migration yet, nothing has to be rolled back.
    return;
  }

  // Turn off all off-by-default services if services were enabled due to
  // unified consent.
  if (user_pref_service->GetBoolean(
          prefs::kAllUnifiedConsentServicesWereEnabled)) {
    service_client->SetServiceEnabled(Service::kSafeBrowsingExtendedReporting,
                                      false);
    service_client->SetServiceEnabled(Service::kSpellCheck, false);
    service_client->SetServiceEnabled(Service::kContextualSearch, false);
  }

  // Clear all unified consent prefs.
  user_pref_service->ClearPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
  user_pref_service->ClearPref(prefs::kUnifiedConsentMigrationState);
  user_pref_service->ClearPref(prefs::kShouldShowUnifiedConsentBump);
  user_pref_service->ClearPref(prefs::kAllUnifiedConsentServicesWereEnabled);
}

void UnifiedConsentService::EnableGoogleServices() {
  DCHECK(identity_manager_->HasPrimaryAccount());
  DCHECK_LT(MigrationState::kNotInitialized, GetMigrationState());

  if (GetMigrationState() != MigrationState::kCompleted) {
    // If the user opted into unified consent, the migration is completed.
    SetMigrationState(MigrationState::kCompleted);
  }

  if (ShouldShowConsentBump())
    RecordConsentBumpSuppressReason(
        metrics::ConsentBumpSuppressReason::kSettingsOptIn);

  // Enable all Google services except sync.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            true);
  for (int i = 0; i <= static_cast<int>(Service::kLast); ++i) {
    Service service = static_cast<Service>(i);
    if (service_client_->GetServiceState(service) !=
        ServiceState::kNotSupported) {
      service_client_->SetServiceEnabled(service, true);
    }
  }

  pref_service_->SetBoolean(prefs::kAllUnifiedConsentServicesWereEnabled, true);
}

bool UnifiedConsentService::ShouldShowConsentBump() {
  if (base::FeatureList::IsEnabled(kForceUnifiedConsentBump) &&
      identity_manager_->HasPrimaryAccount()) {
    return true;
  }
  return pref_service_->GetBoolean(prefs::kShouldShowUnifiedConsentBump);
}

void UnifiedConsentService::MarkConsentBumpShown() {
  // Record suppress reason kNone, which means that it was shown. This also sets
  // the |kShouldShowConsentBump| pref to false.
  RecordConsentBumpSuppressReason(metrics::ConsentBumpSuppressReason::kNone);
}

void UnifiedConsentService::RecordConsentBumpSuppressReason(
    metrics::ConsentBumpSuppressReason suppress_reason) {
  UMA_HISTOGRAM_ENUMERATION("UnifiedConsent.ConsentBump.SuppressReason",
                            suppress_reason);

  switch (suppress_reason) {
    case metrics::ConsentBumpSuppressReason::kNone:
    case metrics::ConsentBumpSuppressReason::kNotSignedIn:
    case metrics::ConsentBumpSuppressReason::kSyncEverythingOff:
    case metrics::ConsentBumpSuppressReason::kPrivacySettingOff:
    case metrics::ConsentBumpSuppressReason::kSettingsOptIn:
    case metrics::ConsentBumpSuppressReason::kUserSignedOut:
    case metrics::ConsentBumpSuppressReason::kUserTurnedSyncDatatypeOff:
    case metrics::ConsentBumpSuppressReason::kUserTurnedPrivacySettingOff:
    case metrics::ConsentBumpSuppressReason::kCustomPassphrase:
      pref_service_->SetBoolean(prefs::kShouldShowUnifiedConsentBump, false);
      break;
    case metrics::ConsentBumpSuppressReason::kSyncPaused:
      // Consent bump should be shown when sync is active again.
      DCHECK(ShouldShowConsentBump());
      break;
  }
}

void UnifiedConsentService::Shutdown() {
  identity_manager_->RemoveObserver(this);
  sync_service_->RemoveObserver(this);
}

void UnifiedConsentService::OnPrimaryAccountCleared(
    const AccountInfo& account_info) {
  pref_service_->SetBoolean(prefs::kAllUnifiedConsentServicesWereEnabled,
                            false);

  // By design, signing out of Chrome automatically disables off-by-default
  // services.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            false);
  service_client_->SetServiceEnabled(Service::kSafeBrowsingExtendedReporting,
                                     false);
  service_client_->SetServiceEnabled(Service::kSpellCheck, false);
  service_client_->SetServiceEnabled(Service::kContextualSearch, false);

  if (GetMigrationState() != MigrationState::kCompleted) {
    // When the user signs out, the migration is complete.
    SetMigrationState(MigrationState::kCompleted);
  }

  if (ShouldShowConsentBump())
    RecordConsentBumpSuppressReason(
        metrics::ConsentBumpSuppressReason::kUserSignedOut);
}

void UnifiedConsentService::OnStateChanged(syncer::SyncService* sync) {
  if (!sync_service_->CanSyncFeatureStart() ||
      !sync_service_->IsEngineInitialized()) {
    return;
  }

  if (GetMigrationState() == MigrationState::kInProgressWaitForSyncInit)
    UpdateSettingsForMigration();

  if (sync_service_->GetUserSettings()->IsUsingSecondaryPassphrase()) {
    if (ShouldShowConsentBump()) {
      // Do not show the consent bump when the user has a custom passphrase.
      RecordConsentBumpSuppressReason(
          metrics::ConsentBumpSuppressReason::kCustomPassphrase);
    }
  }
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
    RecordConsentBumpSuppressReason(
        metrics::ConsentBumpSuppressReason::kNotSignedIn);
    SetMigrationState(MigrationState::kCompleted);
    return;
  }
  bool is_syncing_everything =
      sync_service_->GetUserSettings()->IsSyncEverythingEnabled();

  if (!is_syncing_everything) {
    RecordConsentBumpSuppressReason(
        metrics::ConsentBumpSuppressReason::kSyncEverythingOff);
  } else if (!AreAllOnByDefaultPrivacySettingsOn()) {
    RecordConsentBumpSuppressReason(
        metrics::ConsentBumpSuppressReason::kPrivacySettingOff);
  } else {
    // When the user was syncing everything, and all on-by-default privacy
    // settings were on, the consent bump should be shown.
    pref_service_->SetBoolean(prefs::kShouldShowUnifiedConsentBump, true);
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
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            url_keyed_metrics_enabled);

  SetMigrationState(MigrationState::kCompleted);
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

void UnifiedConsentService::RecordSettingsHistogram() {
  bool metric_recorded = false;

  metric_recorded |= RecordSettingsHistogramFromPref(
      prefs::kAllUnifiedConsentServicesWereEnabled, pref_service_,
      metrics::SettingsHistogramValue::kAllServicesWereEnabled);
  metric_recorded |= RecordSettingsHistogramFromPref(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled, pref_service_,
      metrics::SettingsHistogramValue::kUrlKeyedAnonymizedDataCollection);
  metric_recorded |= RecordSettingsHistogramFromService(
      service_client_.get(),
      UnifiedConsentServiceClient::Service::kSafeBrowsingExtendedReporting,
      metrics::SettingsHistogramValue::kSafeBrowsingExtendedReporting);
  metric_recorded |= RecordSettingsHistogramFromService(
      service_client_.get(), UnifiedConsentServiceClient::Service::kSpellCheck,
      metrics::SettingsHistogramValue::kSpellCheck);

  if (!metric_recorded)
    RecordSettingsHistogramSample(metrics::SettingsHistogramValue::kNone);
}

void UnifiedConsentService::CheckConsentBumpEligibility() {
  // Only check eligility if the user was eligible before.
  if (!ShouldShowConsentBump()) {
    metrics::RecordConsentBumpEligibility(false);
    return;
  }

  if (!sync_service_->GetUserSettings()->GetChosenDataTypes().HasAll(
          syncer::UserSelectableTypes())) {
    RecordConsentBumpSuppressReason(
        metrics::ConsentBumpSuppressReason::kUserTurnedSyncDatatypeOff);
  } else if (!AreAllOnByDefaultPrivacySettingsOn()) {
    RecordConsentBumpSuppressReason(
        metrics::ConsentBumpSuppressReason::kUserTurnedPrivacySettingOff);
  }
  metrics::RecordConsentBumpEligibility(
      pref_service_->GetBoolean(prefs::kShouldShowUnifiedConsentBump));
}

}  //  namespace unified_consent
