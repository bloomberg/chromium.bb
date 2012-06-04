// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/protected_prefs_watcher.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/histograms.h"
#include "chrome/browser/protector/protector_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

namespace protector {

namespace {

// Prefix added to names of backup entries.
const char kBackupPrefsPrefix[] = "backup.";

// Names of prefs that are backed up.
const char* const kProtectedPrefNames[] = {
  prefs::kHomePage,
  prefs::kHomePageIsNewTabPage,
  prefs::kShowHomeButton,
  prefs::kRestoreOnStartup,
  prefs::kURLsToRestoreOnStartup,
  prefs::kPinnedTabs
};

// Backup pref names.
const char kBackupHomePage[] = "backup.homepage";
const char kBackupHomePageIsNewTabPage[] = "backup.homepage_is_newtabpage";
const char kBackupShowHomeButton[] = "backup.browser.show_home_button";
const char kBackupRestoreOnStartup[] = "backup.session.restore_on_startup";
const char kBackupURLsToRestoreOnStartup[] =
    "backup.session.urls_to_restore_on_startup";
const char kBackupPinnedTabs[] = "backup.pinned_tabs";

// Special backup entries.
const char kBackupExtensionsIDs[] = "backup.extensions.ids";
const char kBackupSignature[] = "backup._signature";
const char kBackupVersion[] = "backup._version";

// Returns name of the backup entry for pref |pref_name|.
std::string GetBackupNameFor(const std::string& pref_name) {
  return kBackupPrefsPrefix + pref_name;
}

// Appends a list of strings to |out|.
void StringAppendStringList(const base::ListValue* list, std::string* out) {
  for (base::ListValue::const_iterator it = list->begin(); it != list->end();
       ++it) {
    std::string item;
    if (!(*it)->GetAsString(&item))
      NOTREACHED();
    base::StringAppendF(out, "|%s", item.c_str());
  }
}

// Appends a dictionary with string values to |out|.
void StringAppendStringDictionary(const base::DictionaryValue* dict,
                                  std::string* out) {
  for (base::DictionaryValue::Iterator it(*dict); it.HasNext(); it.Advance()) {
    std::string value;
    if (!it.value().GetAsString(&value))
      NOTREACHED();
    base::StringAppendF(out, "|%s|%s", it.key().c_str(), value.c_str());
  }
}

void StringAppendBoolean(PrefService* prefs,
                         const char* path,
                         std::string* out) {
  if (prefs->HasPrefPath(path))
    base::StringAppendF(out, "|%d", prefs->GetBoolean(path) ? 1 : 0);
  else
    base::StringAppendF(out, "|");
}

void StringAppendInteger(PrefService* prefs,
                         const char* path,
                         std::string* out) {
  if (prefs->HasPrefPath(path))
    base::StringAppendF(out, "|%d", prefs->GetInteger(path));
  else
    base::StringAppendF(out, "|");
}

}  // namespace

// static
const int ProtectedPrefsWatcher::kCurrentVersionNumber = 4;

ProtectedPrefsWatcher::ProtectedPrefsWatcher(Profile* profile)
    : is_backup_valid_(true),
      profile_(profile) {
  // Perform necessary pref migrations before actually starting to observe
  // pref changes, otherwise the migration would affect the backup data as well.
  EnsurePrefsMigration();
  pref_observer_.reset(PrefSetObserver::CreateProtectedPrefSetObserver(
      profile->GetPrefs(), this));
  UpdateCachedPrefs();
  ValidateBackup();
  VLOG(1) << "Initialized pref watcher";
}

ProtectedPrefsWatcher::~ProtectedPrefsWatcher() {
}

// static
void ProtectedPrefsWatcher::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(kBackupHomePage, "",
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(kBackupHomePageIsNewTabPage, false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(kBackupShowHomeButton, false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kBackupRestoreOnStartup, 0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(kBackupURLsToRestoreOnStartup,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(kBackupPinnedTabs,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(kBackupExtensionsIDs,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(kBackupSignature, "",
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kBackupVersion, 1,
                             PrefService::UNSYNCABLE_PREF);
}

bool ProtectedPrefsWatcher::DidPrefChange(const std::string& path) const {
  std::string backup_path = GetBackupNameFor(path);
  PrefService* prefs = profile_->GetPrefs();
  const PrefService::Preference* new_pref = prefs->FindPreference(path.c_str());
  DCHECK(new_pref);
  const PrefService::Preference* backup_pref =
      profile_->GetPrefs()->FindPreference(backup_path.c_str());
  DCHECK(backup_pref);
  if (new_pref->IsDefaultValue())
    return !backup_pref->IsDefaultValue();
  if (!new_pref->IsUserControlled())
    return false;
  return !backup_pref->GetValue()->Equals(new_pref->GetValue());
}

const base::Value* ProtectedPrefsWatcher::GetBackupForPref(
    const std::string& path) const {
  if (!is_backup_valid_)
    return NULL;
  std::string backup_path = GetBackupNameFor(path);
  // These do not directly correspond to any real preference.
  DCHECK(backup_path != kBackupExtensionsIDs &&
         backup_path != kBackupSignature);
  PrefService* prefs = profile_->GetPrefs();
  // If backup is not set, return the default value of the actual pref.
  // TODO(ivankr): return NULL instead and handle appropriately in SettingChange
  // classes.
  if (!prefs->HasPrefPath(backup_path.c_str()))
    return prefs->GetDefaultPrefValue(path.c_str());
  const PrefService::Preference* backup_pref =
      profile_->GetPrefs()->FindPreference(backup_path.c_str());
  DCHECK(backup_pref);
  return backup_pref->GetValue();
}

void ProtectedPrefsWatcher::ForceUpdateBackup() {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramPrefs,
      kProtectorErrorForcedUpdate,
      kProtectorErrorCount);
  InitBackup();
}

void ProtectedPrefsWatcher::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_PREF_CHANGED);
  const std::string* pref_name = content::Details<std::string>(details).ptr();
  DCHECK(pref_name && pref_observer_->IsObserved(*pref_name));
  if (UpdateBackupEntry(*pref_name))
    UpdateBackupSignature();
}

void ProtectedPrefsWatcher::EnsurePrefsMigration() {
  SessionStartupPref::MigrateIfNecessary(profile_->GetPrefs());
}

bool ProtectedPrefsWatcher::UpdateCachedPrefs() {
  // Direct access to the extensions prefs is required becase ExtensionService
  // may not yet have been initialized.
  const base::DictionaryValue* extension_prefs;
  const base::Value* extension_prefs_value =
      profile_->GetPrefs()->GetUserPrefValue(ExtensionPrefs::kExtensionsPref);
  if (!extension_prefs_value ||
      !extension_prefs_value->GetAsDictionary(&extension_prefs)) {
    return false;
  }
  ExtensionPrefs::ExtensionIdSet extension_ids =
      ExtensionPrefs::GetExtensionsFrom(extension_prefs);
  if (extension_ids == cached_extension_ids_)
    return false;
  cached_extension_ids_.swap(extension_ids);
  return true;
}

bool ProtectedPrefsWatcher::HasBackup() const {
  // TODO(ivankr): as soon as some irreversible change to Preferences happens,
  // add a condition that this change has occured as well (otherwise it's
  // possible to simply clear the "backup" dictionary to make settings
  // unprotected).
  return profile_->GetPrefs()->HasPrefPath(kBackupSignature);
}

void ProtectedPrefsWatcher::InitBackup() {
  PrefService* prefs = profile_->GetPrefs();
  for (size_t i = 0; i < arraysize(kProtectedPrefNames); ++i) {
    const base::Value* user_value =
        prefs->GetUserPrefValue(kProtectedPrefNames[i]);
    if (user_value)
      prefs->Set(GetBackupNameFor(kProtectedPrefNames[i]).c_str(), *user_value);
  }
  ListPrefUpdate extension_ids_update(prefs, kBackupExtensionsIDs);
  base::ListValue* extension_ids = extension_ids_update.Get();
  extension_ids->Clear();
  for (ExtensionPrefs::ExtensionIdSet::const_iterator it =
           cached_extension_ids_.begin();
       it != cached_extension_ids_.end(); ++it) {
    extension_ids->Append(base::Value::CreateStringValue(*it));
  }
  prefs->SetInteger(kBackupVersion, kCurrentVersionNumber);
  UpdateBackupSignature();
}

void ProtectedPrefsWatcher::MigrateOldBackupIfNeeded() {
  PrefService* prefs = profile_->GetPrefs();

  int current_version = prefs->GetInteger(kBackupVersion);
  VLOG(1) << "Backup version: " << current_version;
  if (current_version == kCurrentVersionNumber)
    return;

  switch (current_version) {
    case 1: {
        // Add pinned tabs.
        const base::Value* pinned_tabs =
            prefs->GetUserPrefValue(prefs::kPinnedTabs);
        if (pinned_tabs)
          prefs->Set(kBackupPinnedTabs, *pinned_tabs);
      }
      // FALL THROUGH

    case 2:
      // SessionStartupPref migration.
      DCHECK(prefs->GetBoolean(prefs::kRestoreOnStartupMigrated));
      prefs->SetInteger(kBackupRestoreOnStartup,
                        prefs->GetInteger(prefs::kRestoreOnStartup));
      prefs->Set(kBackupURLsToRestoreOnStartup,
                 *prefs->GetList(prefs::kURLsToRestoreOnStartup));
      // FALL THROUGH

    case 3:
      // Reset to default values backup prefs whose actual prefs are not set.
      for (size_t i = 0; i < arraysize(kProtectedPrefNames); ++i) {
        if (!prefs->HasPrefPath(kProtectedPrefNames[i]))
          prefs->ClearPref(GetBackupNameFor(kProtectedPrefNames[i]).c_str());
      }
      // FALL THROUGH
  }

  prefs->SetInteger(kBackupVersion, kCurrentVersionNumber);
  UpdateBackupSignature();
}

bool ProtectedPrefsWatcher::UpdateBackupEntry(const std::string& path) {
  std::string backup_path = GetBackupNameFor(path);
  PrefService* prefs = profile_->GetPrefs();
  const PrefService::Preference* pref = prefs->FindPreference(path.c_str());
  if (path == ExtensionPrefs::kExtensionsPref) {
    // For changes in extension dictionary, do nothing if the IDs list remained
    // the same.
    if (!UpdateCachedPrefs())
      return false;
    ListPrefUpdate extension_ids_update(prefs, kBackupExtensionsIDs);
    base::ListValue* extension_ids = extension_ids_update.Get();
    extension_ids->Clear();
    for (ExtensionPrefs::ExtensionIdSet::const_iterator it =
             cached_extension_ids_.begin();
         it != cached_extension_ids_.end(); ++it) {
      extension_ids->Append(base::Value::CreateStringValue(*it));
    }
  } else if (!prefs->HasPrefPath(path.c_str())) {
    // Preference has been removed, remove the backup as well.
    prefs->ClearPref(backup_path.c_str());
  } else if (!pref->IsUserControlled()) {
    return false;
  } else {
    prefs->Set(backup_path.c_str(), *pref->GetValue());
  }
  VLOG(1) << "Updated backup entry for: " << path;
  return true;
}

void ProtectedPrefsWatcher::UpdateBackupSignature() {
  PrefService* prefs = profile_->GetPrefs();
  std::string signed_data = GetSignatureData(prefs);
  DCHECK(!signed_data.empty());
  std::string signature = SignSetting(signed_data);
  DCHECK(!signature.empty());
  std::string signature_base64;
  if (!base::Base64Encode(signature, &signature_base64))
    NOTREACHED();
  prefs->SetString(kBackupSignature, signature_base64);
  // Schedule disk write on FILE thread as soon as possible.
  prefs->CommitPendingWrite();
  VLOG(1) << "Updated backup signature";
}

bool ProtectedPrefsWatcher::IsSignatureValid() const {
  DCHECK(HasBackup());
  PrefService* prefs = profile_->GetPrefs();
  std::string signed_data = GetSignatureData(prefs);
  DCHECK(!signed_data.empty());
  std::string signature;
  if (!base::Base64Decode(prefs->GetString(kBackupSignature), &signature))
    return false;
  return IsSettingValid(signed_data, signature);
}

void ProtectedPrefsWatcher::ValidateBackup() {
  if (!HasBackup()) {
    // Create initial backup entries and sign them.
    InitBackup();
    UMA_HISTOGRAM_ENUMERATION(
        kProtectorHistogramPrefs,
        kProtectorErrorValueValidZero,
        kProtectorErrorCount);
  } else if (IsSignatureValid()) {
    MigrateOldBackupIfNeeded();
    UMA_HISTOGRAM_ENUMERATION(
        kProtectorHistogramPrefs,
        kProtectorErrorValueValid,
        kProtectorErrorCount);
  } else {
    LOG(WARNING) << "Invalid backup signature";
    is_backup_valid_ = false;
    // The whole backup has been compromised, overwrite it.
    InitBackup();
    UMA_HISTOGRAM_ENUMERATION(
        kProtectorHistogramPrefs,
        kProtectorErrorBackupInvalid,
        kProtectorErrorCount);
  }
}

std::string ProtectedPrefsWatcher::GetSignatureData(PrefService* prefs) const {
  int current_version = prefs->GetInteger(kBackupVersion);
  // TODO(ivankr): replace this with some existing reliable serializer.
  // JSONWriter isn't a good choice because JSON formatting may change suddenly.
  std::string data = prefs->GetString(kBackupHomePage);
  StringAppendBoolean(prefs, kBackupHomePageIsNewTabPage, &data);
  StringAppendBoolean(prefs, kBackupShowHomeButton, &data);
  StringAppendInteger(prefs, kBackupRestoreOnStartup, &data);
  StringAppendStringList(prefs->GetList(kBackupURLsToRestoreOnStartup), &data);
  StringAppendStringList(prefs->GetList(kBackupExtensionsIDs), &data);
  if (current_version >= 2) {
    // Version itself is included only since version 2 since it wasn't there
    // in version 1.
    base::StringAppendF(&data, "|v%d", current_version);
    const base::ListValue* pinned_tabs = prefs->GetList(kBackupPinnedTabs);
    for (base::ListValue::const_iterator it = pinned_tabs->begin();
         it != pinned_tabs->end(); ++it) {
      const base::DictionaryValue* tab = NULL;
      if (!(*it)->GetAsDictionary(&tab)) {
        NOTREACHED();
        continue;
      }
      StringAppendStringDictionary(tab, &data);
    }
  }
  return data;
}

}  // namespace protector
