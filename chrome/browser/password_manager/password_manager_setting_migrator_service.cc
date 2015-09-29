// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_setting_migrator_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_manager_settings_migration_experiment.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/browser/notification_source.h"

namespace {

bool GetBooleanUserOrDefaultPrefValue(PrefService* prefs,
                                      const std::string& name) {
  bool result = false;
  const base::Value* value = prefs->GetUserPrefValue(name);
  if (!value)
    value = prefs->GetDefaultPrefValue(name);
  value->GetAsBoolean(&result);
  return result;
}

void ChangeOnePrefBecauseAnotherPrefHasChanged(
    PrefService* prefs,
    const std::string& other_pref_name,
    const std::string& changed_pref_name) {
  bool changed_pref =
      GetBooleanUserOrDefaultPrefValue(prefs, changed_pref_name);
  bool other_pref = GetBooleanUserOrDefaultPrefValue(prefs, other_pref_name);
  if (changed_pref != other_pref)
    prefs->SetBoolean(other_pref_name, changed_pref);
  // TODO(melandory): add histograms in order to track when we can stop
  // migration.
}

// Change value of both kPasswordManagerSavingEnabled and
// kCredentialsEnableService to the |new_value|.
void UpdatePreferencesValues(PrefService* prefs, bool new_value) {
  prefs->SetBoolean(password_manager::prefs::kPasswordManagerSavingEnabled,
                    new_value);
  prefs->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                    new_value);
}

void SaveCurrentPrefState(PrefService* prefs,
                          bool* new_pref_value,
                          bool* legacy_pref_value) {
  *new_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kCredentialsEnableService);
  *legacy_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kPasswordManagerSavingEnabled);
}

}  // namespace

// static
PasswordManagerSettingMigratorService::Factory*
PasswordManagerSettingMigratorService::Factory::GetInstance() {
  return base::Singleton<PasswordManagerSettingMigratorService::Factory>::get();
}

// static
PasswordManagerSettingMigratorService*
PasswordManagerSettingMigratorService::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<PasswordManagerSettingMigratorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasswordManagerSettingMigratorService::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "PasswordManagerSettingMigratorService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

PasswordManagerSettingMigratorService::Factory::~Factory() {}

KeyedService*
PasswordManagerSettingMigratorService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PasswordManagerSettingMigratorService(
      Profile::FromBrowserContext(context));
}

bool PasswordManagerSettingMigratorService::Factory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

PasswordManagerSettingMigratorService::PasswordManagerSettingMigratorService(
    Profile* profile)
    : profile_(profile), sync_service_(nullptr) {
  SaveCurrentPrefState(profile->GetPrefs(), &initial_new_pref_value_,
                       &initial_legacy_pref_value_);
  // If there will be a ProfileSyncService, the rest of the initialization
  // should take place after that service is created. The ProfileSyncService is
  // normally created before NOTIFICATION_PROFILE_ADDED, so defer the rest of
  // the initialization until after that.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
                 content::Source<Profile>(profile));
}

PasswordManagerSettingMigratorService::
    ~PasswordManagerSettingMigratorService() {}

void PasswordManagerSettingMigratorService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_ADDED, type);
  SaveCurrentPrefState(profile_->GetPrefs(), &initial_new_pref_value_,
                       &initial_legacy_pref_value_);
  const int kMaxInitialValues = 4;
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SettingsReconciliation.InitialValues",
      (static_cast<int>(initial_new_pref_value_) << 1 |
       static_cast<int>(initial_legacy_pref_value_)),
      kMaxInitialValues);
  if (!password_manager::IsSettingsMigrationActive()) {
    return;
  }
  if (ProfileSyncServiceFactory::HasProfileSyncService(profile_))
    sync_service_ = ProfileSyncServiceFactory::GetForProfile(profile_);
  if (!CanSyncStart())
    MigrateOffState(profile_->GetPrefs());
  InitObservers();
}

void PasswordManagerSettingMigratorService::InitObservers() {
  syncable_prefs::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile_);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      password_manager::prefs::kCredentialsEnableService,
      base::Bind(&PasswordManagerSettingMigratorService::
                     OnCredentialsEnableServicePrefChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      password_manager::prefs::kPasswordManagerSavingEnabled,
      base::Bind(&PasswordManagerSettingMigratorService::
                     OnPasswordManagerSavingEnabledPrefChanged,
                 base::Unretained(this)));
  // This causes OnIsSyncingChanged to be called when the value of
  // PrefService::IsSyncing() changes.
  prefs->AddObserver(this);
}

bool PasswordManagerSettingMigratorService::CanSyncStart() {
  return sync_service_ && sync_service_->CanSyncStart() &&
         syncer::UserSelectableTypes().Has(syncer::PREFERENCES);
}

void PasswordManagerSettingMigratorService::Shutdown() {
  syncable_prefs::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile_);
  prefs->RemoveObserver(this);
}

void PasswordManagerSettingMigratorService::
    OnCredentialsEnableServicePrefChanged(
        const std::string& changed_pref_name) {
  PrefService* prefs = profile_->GetPrefs();
  sync_data_.push_back(GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kCredentialsEnableService));
  ChangeOnePrefBecauseAnotherPrefHasChanged(
      prefs, password_manager::prefs::kPasswordManagerSavingEnabled,
      password_manager::prefs::kCredentialsEnableService);
}

void PasswordManagerSettingMigratorService::
    OnPasswordManagerSavingEnabledPrefChanged(
        const std::string& changed_pref_name) {
  PrefService* prefs = profile_->GetPrefs();
  sync_data_.push_back(GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kPasswordManagerSavingEnabled));
  ChangeOnePrefBecauseAnotherPrefHasChanged(
      prefs, password_manager::prefs::kCredentialsEnableService,
      password_manager::prefs::kPasswordManagerSavingEnabled);
}

void PasswordManagerSettingMigratorService::OnIsSyncingChanged() {
  syncable_prefs::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile_);
  if (prefs->IsSyncing() && prefs->IsPrioritySyncing()) {
    // Initial sync has finished.
    MigrateAfterModelAssociation(prefs);
  }

  if (prefs->IsSyncing() == prefs->IsPrioritySyncing()) {
    // Sync is not in model association step.
    SaveCurrentPrefState(prefs, &initial_new_pref_value_,
                         &initial_legacy_pref_value_);
    sync_data_.clear();
  }
}

void PasswordManagerSettingMigratorService::MigrateOffState(
    PrefService* prefs) {
  bool new_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kCredentialsEnableService);
  bool legacy_pref_value = GetBooleanUserOrDefaultPrefValue(
      prefs, password_manager::prefs::kPasswordManagerSavingEnabled);
  UpdatePreferencesValues(prefs, new_pref_value && legacy_pref_value);
}

void PasswordManagerSettingMigratorService::MigrateAfterModelAssociation(
    PrefService* prefs) {
  if (sync_data_.empty()) {
    MigrateOffState(prefs);
  } else if (sync_data_.size() == 1) {
    // Only one value came from sync. This value should be assigned to both
    // preferences.
    UpdatePreferencesValues(prefs, sync_data_[0]);
  } else {
    bool sync_new_pref_value = sync_data_[0];
    bool sync_legacy_pref_value = sync_data_.back();
    if (sync_legacy_pref_value && sync_new_pref_value) {
      UpdatePreferencesValues(prefs, true);
    } else if (!sync_legacy_pref_value && !sync_new_pref_value) {
      UpdatePreferencesValues(prefs, false);
    } else if (!initial_legacy_pref_value_ && !initial_new_pref_value_) {
      UpdatePreferencesValues(prefs, true);
    } else {
      UpdatePreferencesValues(prefs, false);
    }
  }
  // TODO(melandory) Add histogram which will log combination of initial and
  // final values for the both preferences.
}
