// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  RegisterPrefGroups();
  // TODO(tim): Create a Mock instead of maintaining the if(!pref_service_) case
  // throughout this file.  This is a problem now due to lack of injection at
  // ProfileSyncService. Bug 130176.
  if (pref_service_) {
    RegisterPreferences();
    // Watch the preference that indicates sync is managed so we can take
    // appropriate action.
    pref_sync_managed_.Init(prefs::kSyncManaged, pref_service_, this);
  }
}

SyncPrefs::~SyncPrefs() {
  DCHECK(CalledOnValidThread());
}

void SyncPrefs::AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK(CalledOnValidThread());
  sync_pref_observers_.AddObserver(sync_pref_observer);
}

void SyncPrefs::RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK(CalledOnValidThread());
  sync_pref_observers_.RemoveObserver(sync_pref_observer);
}

void SyncPrefs::ClearPreferences() {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service_->ClearPref(prefs::kSyncHasSetupCompleted);
  pref_service_->ClearPref(prefs::kSyncEncryptionBootstrapToken);

  // TODO(nick): The current behavior does not clear
  // e.g. prefs::kSyncBookmarks.  Is that really what we want?
}

bool SyncPrefs::HasSyncSetupCompleted() const {
  DCHECK(CalledOnValidThread());
  return
      pref_service_ &&
      pref_service_->GetBoolean(prefs::kSyncHasSetupCompleted);
}

void SyncPrefs::SetSyncSetupCompleted() {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kSyncHasSetupCompleted, true);
  SetStartSuppressed(false);
}

bool SyncPrefs::IsStartSuppressed() const {
  DCHECK(CalledOnValidThread());
  return
      pref_service_ &&
      pref_service_->GetBoolean(prefs::kSyncSuppressStart);
}

void SyncPrefs::SetStartSuppressed(bool is_suppressed) {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kSyncSuppressStart, is_suppressed);
}

std::string SyncPrefs::GetGoogleServicesUsername() const {
  DCHECK(CalledOnValidThread());
  return
      pref_service_ ?
      pref_service_->GetString(prefs::kGoogleServicesUsername) : "";
}

base::Time SyncPrefs::GetLastSyncedTime() const {
  DCHECK(CalledOnValidThread());
  return
      base::Time::FromInternalValue(
          pref_service_ ?
          pref_service_->GetInt64(prefs::kSyncLastSyncedTime) : 0);
}

void SyncPrefs::SetLastSyncedTime(base::Time time) {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetInt64(prefs::kSyncLastSyncedTime, time.ToInternalValue());
}

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK(CalledOnValidThread());
  return
      pref_service_ &&
      pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced);
}

void SyncPrefs::SetKeepEverythingSynced(bool keep_everything_synced) {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kSyncKeepEverythingSynced,
                            keep_everything_synced);
}

syncable::ModelTypeSet SyncPrefs::GetPreferredDataTypes(
    syncable::ModelTypeSet registered_types) const {
  DCHECK(CalledOnValidThread());
  if (!pref_service_) {
    return syncable::ModelTypeSet();
  }

  // First remove any datatypes that are inconsistent with the current policies
  // on the client (so that "keep everything synced" doesn't include them).
  if (pref_service_->HasPrefPath(prefs::kSavingBrowserHistoryDisabled) &&
      pref_service_->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    registered_types.Remove(syncable::TYPED_URLS);
  }

  if (pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced)) {
    return registered_types;
  }

  syncable::ModelTypeSet preferred_types;
  for (syncable::ModelTypeSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    if (GetDataTypePreferred(it.Get())) {
      preferred_types.Put(it.Get());
    }
  }
  return ResolvePrefGroups(registered_types, preferred_types);
}

void SyncPrefs::SetPreferredDataTypes(
    syncable::ModelTypeSet registered_types,
    syncable::ModelTypeSet preferred_types) {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  DCHECK(registered_types.HasAll(preferred_types));
  preferred_types = ResolvePrefGroups(registered_types, preferred_types);
  for (syncable::ModelTypeSet::Iterator i = registered_types.First();
       i.Good(); i.Inc()) {
    SetDataTypePreferred(i.Get(), preferred_types.Has(i.Get()));
  }
}

bool SyncPrefs::IsManaged() const {
  DCHECK(CalledOnValidThread());
  return pref_service_ && pref_service_->GetBoolean(prefs::kSyncManaged);
}

std::string SyncPrefs::GetEncryptionBootstrapToken() const {
  DCHECK(CalledOnValidThread());
  return
      pref_service_ ?
      pref_service_->GetString(prefs::kSyncEncryptionBootstrapToken) : "";
}

void SyncPrefs::SetEncryptionBootstrapToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncEncryptionBootstrapToken, token);
}

#if defined(OS_CHROMEOS)
std::string SyncPrefs::GetSpareBootstrapToken() const {
  DCHECK(CalledOnValidThread());
  return pref_service_ ?
      pref_service_->GetString(prefs::kSyncSpareBootstrapToken) : "";
}

void SyncPrefs::SetSpareBootstrapToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncSpareBootstrapToken, token);
}
#endif

void SyncPrefs::AcknowledgeSyncedTypes(
    syncable::ModelTypeSet types) {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  // Add the types to the current set of acknowledged
  // types, and then store the resulting set in prefs.
  const syncable::ModelTypeSet acknowledged_types =
      Union(types,
            syncable::ModelTypeSetFromValue(
                *pref_service_->GetList(prefs::kSyncAcknowledgedSyncTypes)));

  scoped_ptr<ListValue> value(
      syncable::ModelTypeSetToValue(acknowledged_types));
  pref_service_->Set(prefs::kSyncAcknowledgedSyncTypes, *value);
}

void SyncPrefs::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
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
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kSyncManaged, is_managed);
}

syncable::ModelTypeSet SyncPrefs::GetAcknowledgeSyncedTypesForTest() const {
  DCHECK(CalledOnValidThread());
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

void SyncPrefs::RegisterPrefGroups() {
  pref_groups_[syncable::APPS].Put(syncable::APP_NOTIFICATIONS);
  pref_groups_[syncable::APPS].Put(syncable::APP_SETTINGS);

  pref_groups_[syncable::AUTOFILL].Put(syncable::AUTOFILL_PROFILE);

  pref_groups_[syncable::EXTENSIONS].Put(syncable::EXTENSION_SETTINGS);

  pref_groups_[syncable::PREFERENCES].Put(syncable::SEARCH_ENGINES);
}

void SyncPrefs::RegisterPreferences() {
  DCHECK(CalledOnValidThread());
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
#if defined(OS_CHROMEOS)
  pref_service_->RegisterStringPref(prefs::kSyncSpareBootstrapToken,
                                    "",
                                    PrefService::UNSYNCABLE_PREF);
#endif

  // We will start prompting people about new data types after the launch of
  // SESSIONS - all previously launched data types are treated as if they are
  // already acknowledged.
  syncable::ModelTypeSet model_set;
  model_set.Put(syncable::BOOKMARKS);
  model_set.Put(syncable::PREFERENCES);
  model_set.Put(syncable::PASSWORDS);
  model_set.Put(syncable::AUTOFILL_PROFILE);
  model_set.Put(syncable::AUTOFILL);
  model_set.Put(syncable::THEMES);
  model_set.Put(syncable::EXTENSIONS);
  model_set.Put(syncable::NIGORI);
  model_set.Put(syncable::SEARCH_ENGINES);
  model_set.Put(syncable::APPS);
  model_set.Put(syncable::TYPED_URLS);
  model_set.Put(syncable::SESSIONS);
  pref_service_->RegisterListPref(prefs::kSyncAcknowledgedSyncTypes,
                                  syncable::ModelTypeSetToValue(model_set),
                                  PrefService::UNSYNCABLE_PREF);
}

void SyncPrefs::RegisterDataTypePreferredPref(syncable::ModelType type,
                                              bool is_preferred) {
  DCHECK(CalledOnValidThread());
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
  DCHECK(CalledOnValidThread());
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
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return;
  }
  pref_service_->SetBoolean(pref_name, is_preferred);
}

syncable::ModelTypeSet SyncPrefs::ResolvePrefGroups(
    syncable::ModelTypeSet registered_types,
    syncable::ModelTypeSet types) const {
  DCHECK(registered_types.HasAll(types));
  syncable::ModelTypeSet types_with_groups = types;
  for (PrefGroupsMap::const_iterator i = pref_groups_.begin();
      i != pref_groups_.end(); ++i) {
    if (types.Has(i->first))
      types_with_groups.PutAll(i->second);
    else
      types_with_groups.RemoveAll(i->second);
  }
  types_with_groups.RetainAll(registered_types);
  return types_with_groups;
}

}  // namespace browser_sync
