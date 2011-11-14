// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_prefs.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace browser_sync {

SyncPrefObserver::~SyncPrefObserver() {}

SyncPrefs::SyncPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
  if (pref_service_) {
    RegisterPreferences();
    // Watch the preference that indicates sync is managed so we can take
    // appropriate action.
    pref_sync_managed_.Init(prefs::kSyncManaged, pref_service_, this);
  }
}

SyncPrefs::~SyncPrefs() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void SyncPrefs::AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  sync_pref_observers_.AddObserver(sync_pref_observer);
}

void SyncPrefs::RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  sync_pref_observers_.RemoveObserver(sync_pref_observer);
}

void SyncPrefs::ClearPreferences() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service_->ClearPref(prefs::kSyncHasSetupCompleted);
  pref_service_->ClearPref(prefs::kSyncEncryptionBootstrapToken);

  // TODO(nick): The current behavior does not clear
  // e.g. prefs::kSyncBookmarks.  Is that really what we want?

  pref_service_->ClearPref(prefs::kSyncMaxInvalidationVersions);

  pref_service_->ScheduleSavePersistentPrefs();
}

bool SyncPrefs::HasSyncSetupCompleted() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return
      pref_service_ &&
      pref_service_->GetBoolean(prefs::kSyncHasSetupCompleted);
}

void SyncPrefs::SetSyncSetupCompleted() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kSyncHasSetupCompleted, true);
  SetStartSuppressed(false);
  pref_service_->ScheduleSavePersistentPrefs();
}

bool SyncPrefs::IsStartSuppressed() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return
      pref_service_ &&
      pref_service_->GetBoolean(prefs::kSyncSuppressStart);
}

void SyncPrefs::SetStartSuppressed(bool is_suppressed) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kSyncSuppressStart, is_suppressed);
  pref_service_->ScheduleSavePersistentPrefs();
}

std::string SyncPrefs::GetGoogleServicesUsername() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return
      pref_service_ ?
      pref_service_->GetString(prefs::kGoogleServicesUsername) : "";
}

base::Time SyncPrefs::GetLastSyncedTime() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return
      base::Time::FromInternalValue(
          pref_service_ ?
          pref_service_->GetInt64(prefs::kSyncLastSyncedTime) : 0);
}

void SyncPrefs::SetLastSyncedTime(base::Time time) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetInt64(prefs::kSyncLastSyncedTime, time.ToInternalValue());
  pref_service_->ScheduleSavePersistentPrefs();
}

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return
      pref_service_ &&
      pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced);
}

void SyncPrefs::SetKeepEverythingSynced(bool keep_everything_synced) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kSyncKeepEverythingSynced,
                            keep_everything_synced);
  pref_service_->ScheduleSavePersistentPrefs();
}

syncable::ModelTypeSet SyncPrefs::GetPreferredDataTypes(
    const syncable::ModelTypeSet& registered_types) const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!pref_service_) {
    return syncable::ModelTypeSet();
  }

  if (pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced)) {
    return registered_types;
  }

  // Remove autofill_profile since it's controlled by autofill, and
  // search_engines since it's controlled by preferences (see code below).
  syncable::ModelTypeSet user_selectable_types(registered_types);
  DCHECK_EQ(user_selectable_types.count(syncable::NIGORI), 0u);
  user_selectable_types.erase(syncable::AUTOFILL_PROFILE);
  user_selectable_types.erase(syncable::SEARCH_ENGINES);

  // Remove app_notifications since it's controlled by apps (see
  // code below).
  // TODO(akalin): Centralize notion of all user selectable data types.
  user_selectable_types.erase(syncable::APP_NOTIFICATIONS);

  syncable::ModelTypeSet preferred_types;

  for (syncable::ModelTypeSet::const_iterator it =
           user_selectable_types.begin();
       it != user_selectable_types.end(); ++it) {
    if (GetDataTypePreferred(*it)) {
      preferred_types.insert(*it);
    }
  }

  // Group the enabled/disabled state of autofill_profile with autofill, and
  // search_engines with preferences (since only autofill and preferences are
  // shown on the UI).
  if (registered_types.count(syncable::AUTOFILL) &&
      registered_types.count(syncable::AUTOFILL_PROFILE) &&
      GetDataTypePreferred(syncable::AUTOFILL)) {
    preferred_types.insert(syncable::AUTOFILL_PROFILE);
  }
  if (registered_types.count(syncable::PREFERENCES) &&
      registered_types.count(syncable::SEARCH_ENGINES) &&
      GetDataTypePreferred(syncable::PREFERENCES)) {
    preferred_types.insert(syncable::SEARCH_ENGINES);
  }

  // Set app_notifications to the same enabled/disabled state as
  // apps (since only apps is shown on the UI).
  if (registered_types.count(syncable::APPS) &&
      registered_types.count(syncable::APP_NOTIFICATIONS) &&
      GetDataTypePreferred(syncable::APPS)) {
    preferred_types.insert(syncable::APP_NOTIFICATIONS);
  }

  return preferred_types;
}

void SyncPrefs::SetPreferredDataTypes(
    const syncable::ModelTypeSet& registered_types,
    const syncable::ModelTypeSet& preferred_types) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  DCHECK(std::includes(registered_types.begin(), registered_types.end(),
                       preferred_types.begin(), preferred_types.end()));
  syncable::ModelTypeSet preferred_types_with_dependents(preferred_types);
  // Set autofill_profile to the same enabled/disabled state as
  // autofill (since only autofill is shown in the UI).
  if (registered_types.count(syncable::AUTOFILL) &&
      registered_types.count(syncable::AUTOFILL_PROFILE)) {
    if (preferred_types_with_dependents.count(syncable::AUTOFILL)) {
      preferred_types_with_dependents.insert(syncable::AUTOFILL_PROFILE);
    } else {
      preferred_types_with_dependents.erase(syncable::AUTOFILL_PROFILE);
    }
  }
  // Set app_notifications to the same enabled/disabled state as
  // apps (since only apps is shown in the UI).
  if (registered_types.count(syncable::APPS) &&
      registered_types.count(syncable::APP_NOTIFICATIONS)) {
    if (preferred_types_with_dependents.count(syncable::APPS)) {
      preferred_types_with_dependents.insert(syncable::APP_NOTIFICATIONS);
    } else {
      preferred_types_with_dependents.erase(syncable::APP_NOTIFICATIONS);
    }
  }
  // Set search_engines to the same enabled/disabled state as
  // preferences (since only preferences is shown in the UI).
  if (registered_types.count(syncable::PREFERENCES) &&
      registered_types.count(syncable::SEARCH_ENGINES)) {
    if (preferred_types_with_dependents.count(syncable::PREFERENCES)) {
      preferred_types_with_dependents.insert(syncable::SEARCH_ENGINES);
    } else {
      preferred_types_with_dependents.erase(syncable::SEARCH_ENGINES);
    }
  }

  for (syncable::ModelTypeSet::const_iterator it = registered_types.begin();
       it != registered_types.end(); ++it) {
    SetDataTypePreferred(*it, preferred_types_with_dependents.count(*it) > 0);
  }
}

bool SyncPrefs::IsManaged() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return pref_service_ && pref_service_->GetBoolean(prefs::kSyncManaged);
}

std::string SyncPrefs::GetEncryptionBootstrapToken() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return
      pref_service_ ?
      pref_service_->GetString(prefs::kSyncEncryptionBootstrapToken) : "";
}

void SyncPrefs::SetEncryptionBootstrapToken(const std::string& token) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetString(prefs::kSyncEncryptionBootstrapToken, token);
  pref_service_->ScheduleSavePersistentPrefs();
}

sync_notifier::InvalidationVersionMap SyncPrefs::GetAllMaxVersions() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!pref_service_) {
    return sync_notifier::InvalidationVersionMap();
  }
  // Complicated gross code to convert from a string -> string
  // DictionaryValue to a ModelType -> int64 map.
  const base::DictionaryValue* max_versions_dict =
      pref_service_->GetDictionary(prefs::kSyncMaxInvalidationVersions);
  CHECK(max_versions_dict);
  sync_notifier::InvalidationVersionMap max_versions;
  for (base::DictionaryValue::key_iterator it =
           max_versions_dict->begin_keys();
       it != max_versions_dict->end_keys(); ++it) {
    int model_type_int = 0;
    if (!base::StringToInt(*it, &model_type_int)) {
      LOG(WARNING) << "Invalid model type key: " << *it;
      continue;
    }
    if ((model_type_int < syncable::FIRST_REAL_MODEL_TYPE) ||
        (model_type_int >= syncable::MODEL_TYPE_COUNT)) {
      LOG(WARNING) << "Out-of-range model type key: " << model_type_int;
      continue;
    }
    const syncable::ModelType model_type =
        syncable::ModelTypeFromInt(model_type_int);
    std::string max_version_str;
    CHECK(max_versions_dict->GetString(*it, &max_version_str));
    int64 max_version = 0;
    if (!base::StringToInt64(max_version_str, &max_version)) {
      LOG(WARNING) << "Invalid max invalidation version for "
                   << syncable::ModelTypeToString(model_type) << ": "
                   << max_version_str;
      continue;
    }
    max_versions[model_type] = max_version;
  }
  return max_versions;
}

void SyncPrefs::SetMaxVersion(syncable::ModelType model_type,
                              int64 max_version) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(syncable::IsRealDataType(model_type));
  CHECK(pref_service_);
  sync_notifier::InvalidationVersionMap max_versions =
      GetAllMaxVersions();
  sync_notifier::InvalidationVersionMap::iterator it =
      max_versions.find(model_type);
  if ((it != max_versions.end()) && (max_version <= it->second)) {
    NOTREACHED();
    return;
  }
  max_versions[model_type] = max_version;

  // Gross code to convert from a ModelType -> int64 map to a string
  // -> string DictionaryValue.
  base::DictionaryValue max_versions_dict;
  for (sync_notifier::InvalidationVersionMap::const_iterator it =
           max_versions.begin();
       it != max_versions.end(); ++it) {
    max_versions_dict.SetString(
        base::IntToString(it->first),
        base::Int64ToString(it->second));
  }
  pref_service_->Set(prefs::kSyncMaxInvalidationVersions, max_versions_dict);
}

void SyncPrefs::AcknowledgeSyncedTypes(
    const syncable::ModelTypeSet& types) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  syncable::ModelTypeSet acknowledged_types =
      syncable::ModelTypeSetFromValue(
          *pref_service_->GetList(prefs::kSyncAcknowledgedSyncTypes));

  // Add the types to the current set of acknowledged
  // types, and then store the resulting set in prefs.
  {
    syncable::ModelTypeSet temp;
    std::set_union(acknowledged_types.begin(), acknowledged_types.end(),
                   types.begin(), types.end(),
                   std::inserter(temp, temp.end()));
    std::swap(acknowledged_types, temp);
  }

  scoped_ptr<ListValue> value(
      syncable::ModelTypeSetToValue(acknowledged_types));
  pref_service_->Set(prefs::kSyncAcknowledgedSyncTypes, *value);
  pref_service_->ScheduleSavePersistentPrefs();
}

void SyncPrefs::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(content::Source<PrefService>(pref_service_) == source);
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      const std::string* pref_name =
          content::Details<const std::string>(details).ptr();
      if (*pref_name == prefs::kSyncManaged) {
        FOR_EACH_OBSERVER(SyncPrefObserver, sync_pref_observers_,
                          OnSyncManagedPrefChange(*pref_sync_managed_));
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void SyncPrefs::SetManagedForTest(bool is_managed) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kSyncManaged, is_managed);
  pref_service_->ScheduleSavePersistentPrefs();
}

syncable::ModelTypeSet SyncPrefs::GetAcknowledgeSyncedTypesForTest() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!pref_service_) {
    return syncable::ModelTypeSet();
  }
  return syncable::ModelTypeSetFromValue(
      *pref_service_->GetList(prefs::kSyncAcknowledgedSyncTypes));
}

namespace {

const char* GetPrefNameForDataType(syncable::ModelType data_type) {
  switch (data_type) {
    case syncable::BOOKMARKS:
      return prefs::kSyncBookmarks;
    case syncable::PASSWORDS:
      return prefs::kSyncPasswords;
    case syncable::PREFERENCES:
      return prefs::kSyncPreferences;
    case syncable::AUTOFILL:
      return prefs::kSyncAutofill;
    case syncable::AUTOFILL_PROFILE:
      return prefs::kSyncAutofillProfile;
    case syncable::THEMES:
      return prefs::kSyncThemes;
    case syncable::TYPED_URLS:
      return prefs::kSyncTypedUrls;
    case syncable::EXTENSION_SETTINGS:
      return prefs::kSyncExtensionSettings;
    case syncable::EXTENSIONS:
      return prefs::kSyncExtensions;
    case syncable::APP_SETTINGS:
      return prefs::kSyncAppSettings;
    case syncable::APPS:
      return prefs::kSyncApps;
    case syncable::SEARCH_ENGINES:
      return prefs::kSyncSearchEngines;
    case syncable::SESSIONS:
      return prefs::kSyncSessions;
    case syncable::APP_NOTIFICATIONS:
      return prefs::kSyncAppNotifications;
    default:
      break;
  }
  NOTREACHED();
  return NULL;
}

}  // namespace

void SyncPrefs::RegisterPreferences() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  if (pref_service_->FindPreference(prefs::kSyncLastSyncedTime)) {
    return;
  }

  pref_service_->RegisterBooleanPref(prefs::kSyncHasSetupCompleted,
                                     false,
                                     PrefService::UNSYNCABLE_PREF);
  pref_service_->RegisterBooleanPref(prefs::kSyncSuppressStart,
                                     false,
                                     PrefService::UNSYNCABLE_PREF);
  pref_service_->RegisterInt64Pref(prefs::kSyncLastSyncedTime,
                                   0,
                                   PrefService::UNSYNCABLE_PREF);

  // If you've never synced before, or if you're using Chrome OS, all datatypes
  // are on by default.
  // TODO(nick): Perhaps a better model would be to always default to false,
  // and explicitly call SetDataTypes() when the user shows the wizard.
#if defined(OS_CHROMEOS)
  bool enable_by_default = true;
#else
  bool enable_by_default =
      !pref_service_->HasPrefPath(prefs::kSyncHasSetupCompleted);
#endif

  pref_service_->RegisterBooleanPref(prefs::kSyncKeepEverythingSynced,
                                     enable_by_default,
                                     PrefService::UNSYNCABLE_PREF);

  // Treat bookmarks specially.
  RegisterDataTypePreferredPref(syncable::BOOKMARKS, true);
  for (int i = syncable::PREFERENCES; i < syncable::MODEL_TYPE_COUNT; ++i) {
    const syncable::ModelType type = syncable::ModelTypeFromInt(i);
    // Also treat nigori specially.
    if (type == syncable::NIGORI) {
      continue;
    }
    RegisterDataTypePreferredPref(type, enable_by_default);
  }

  pref_service_->RegisterBooleanPref(prefs::kSyncManaged,
                                     false,
                                     PrefService::UNSYNCABLE_PREF);
  pref_service_->RegisterStringPref(prefs::kSyncEncryptionBootstrapToken,
                                    "",
                                    PrefService::UNSYNCABLE_PREF);

  // We will start prompting people about new data types after the launch of
  // SESSIONS - all previously launched data types are treated as if they are
  // already acknowledged.
  syncable::ModelTypeBitSet model_set;
  model_set.set(syncable::BOOKMARKS);
  model_set.set(syncable::PREFERENCES);
  model_set.set(syncable::PASSWORDS);
  model_set.set(syncable::AUTOFILL_PROFILE);
  model_set.set(syncable::AUTOFILL);
  model_set.set(syncable::THEMES);
  model_set.set(syncable::EXTENSIONS);
  model_set.set(syncable::NIGORI);
  model_set.set(syncable::SEARCH_ENGINES);
  model_set.set(syncable::APPS);
  model_set.set(syncable::TYPED_URLS);
  model_set.set(syncable::SESSIONS);
  pref_service_->RegisterListPref(prefs::kSyncAcknowledgedSyncTypes,
                                  syncable::ModelTypeBitSetToValue(model_set),
                                  PrefService::UNSYNCABLE_PREF);

  pref_service_->RegisterDictionaryPref(prefs::kSyncMaxInvalidationVersions,
                                        PrefService::UNSYNCABLE_PREF);
}

void SyncPrefs::RegisterDataTypePreferredPref(syncable::ModelType type,
                                              bool is_preferred) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return;
  }
  pref_service_->RegisterBooleanPref(pref_name, is_preferred,
                                     PrefService::UNSYNCABLE_PREF);
}

bool SyncPrefs::GetDataTypePreferred(syncable::ModelType type) const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!pref_service_) {
    return false;
  }
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return false;
  }

  return pref_service_->GetBoolean(pref_name);
}

void SyncPrefs::SetDataTypePreferred(
    syncable::ModelType type, bool is_preferred) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(pref_service_);
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return;
  }
  pref_service_->SetBoolean(pref_name, is_preferred);
  pref_service_->ScheduleSavePersistentPrefs();
}

}  // namespace browser_sync
