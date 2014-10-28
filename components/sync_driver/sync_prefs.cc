// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/sync_prefs.h"

#include "base/logging.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_driver/pref_names.h"

namespace sync_driver {

SyncPrefObserver::~SyncPrefObserver() {}

SyncPrefs::SyncPrefs(PrefService* pref_service) : pref_service_(pref_service) {
  DCHECK(pref_service);
  RegisterPrefGroups();
  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(
      prefs::kSyncManaged,
      pref_service_,
      base::Bind(&SyncPrefs::OnSyncManagedPrefChanged, base::Unretained(this)));
}

SyncPrefs::SyncPrefs() : pref_service_(NULL) {}

SyncPrefs::~SyncPrefs() { DCHECK(CalledOnValidThread()); }

// static
void SyncPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kSyncHasSetupCompleted,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSyncSuppressStart,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(
      prefs::kSyncLastSyncedTime,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(
      prefs::kSyncFirstSyncTime,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  // All datatypes are on by default, but this gets set explicitly
  // when you configure sync (when turning it on), in
  // ProfileSyncService::OnUserChoseDatatypes.
  registry->RegisterBooleanPref(
      prefs::kSyncKeepEverythingSynced,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  syncer::ModelTypeSet user_types = syncer::UserTypes();

  // Include proxy types as well, as they can be individually selected,
  // although they don't have sync representations.
  user_types.PutAll(syncer::ProxyTypes());

  // Treat bookmarks specially.
  RegisterDataTypePreferredPref(registry, syncer::BOOKMARKS, true);
  user_types.Remove(syncer::BOOKMARKS);

  // These two prefs are set from sync experiment to enable enhanced bookmarks.
  registry->RegisterIntegerPref(
      prefs::kEnhancedBookmarksExperimentEnabled,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterStringPref(
      prefs::kEnhancedBookmarksExtensionId,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  // All types are set to off by default, which forces a configuration to
  // explicitly enable them. GetPreferredTypes() will ensure that any new
  // implicit types are enabled when their pref group is, or via
  // KeepEverythingSynced.
  for (syncer::ModelTypeSet::Iterator it = user_types.First(); it.Good();
       it.Inc()) {
    RegisterDataTypePreferredPref(registry, it.Get(), false);
  }

  registry->RegisterBooleanPref(
      prefs::kSyncManaged,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSyncEncryptionBootstrapToken,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSyncKeystoreEncryptionBootstrapToken,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#if defined(OS_CHROMEOS)
  registry->RegisterStringPref(
      prefs::kSyncSpareBootstrapToken,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif

  registry->RegisterBooleanPref(
      prefs::kSyncHasAuthError,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterStringPref(
      prefs::kSyncSessionsGUID,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  // We will start prompting people about new data types after the launch of
  // SESSIONS - all previously launched data types are treated as if they are
  // already acknowledged.
  syncer::ModelTypeSet model_set;
  model_set.Put(syncer::BOOKMARKS);
  model_set.Put(syncer::PREFERENCES);
  model_set.Put(syncer::PASSWORDS);
  model_set.Put(syncer::AUTOFILL_PROFILE);
  model_set.Put(syncer::AUTOFILL);
  model_set.Put(syncer::THEMES);
  model_set.Put(syncer::EXTENSIONS);
  model_set.Put(syncer::NIGORI);
  model_set.Put(syncer::SEARCH_ENGINES);
  model_set.Put(syncer::APPS);
  model_set.Put(syncer::APP_LIST);
  model_set.Put(syncer::TYPED_URLS);
  model_set.Put(syncer::SESSIONS);
  model_set.Put(syncer::ARTICLES);
  registry->RegisterListPref(prefs::kSyncAcknowledgedSyncTypes,
                             syncer::ModelTypeSetToValue(model_set),
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterIntegerPref(
      prefs::kSyncRemainingRollbackTries, 0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
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
  pref_service_->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service_->ClearPref(prefs::kSyncHasSetupCompleted);
  pref_service_->ClearPref(prefs::kSyncEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncKeystoreEncryptionBootstrapToken);

  // TODO(nick): The current behavior does not clear
  // e.g. prefs::kSyncBookmarks.  Is that really what we want?
}

bool SyncPrefs::HasSyncSetupCompleted() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncHasSetupCompleted);
}

void SyncPrefs::SetSyncSetupCompleted() {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncHasSetupCompleted, true);
  SetStartSuppressed(false);
}

bool SyncPrefs::SyncHasAuthError() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncHasAuthError);
}

void SyncPrefs::SetSyncAuthError(bool error) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncHasAuthError, error);
}

bool SyncPrefs::IsStartSuppressed() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncSuppressStart);
}

void SyncPrefs::SetStartSuppressed(bool is_suppressed) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncSuppressStart, is_suppressed);
}

base::Time SyncPrefs::GetLastSyncedTime() const {
  DCHECK(CalledOnValidThread());
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncLastSyncedTime));
}

void SyncPrefs::SetLastSyncedTime(base::Time time) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetInt64(prefs::kSyncLastSyncedTime, time.ToInternalValue());
}

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced);
}

void SyncPrefs::SetKeepEverythingSynced(bool keep_everything_synced) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncKeepEverythingSynced,
                            keep_everything_synced);
}

syncer::ModelTypeSet SyncPrefs::GetPreferredDataTypes(
    syncer::ModelTypeSet registered_types) const {
  DCHECK(CalledOnValidThread());

  if (pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced)) {
    return registered_types;
  }

  syncer::ModelTypeSet preferred_types;
  for (syncer::ModelTypeSet::Iterator it = registered_types.First(); it.Good();
       it.Inc()) {
    if (GetDataTypePreferred(it.Get())) {
      preferred_types.Put(it.Get());
    }
  }
  return ResolvePrefGroups(registered_types, preferred_types);
}

void SyncPrefs::SetPreferredDataTypes(syncer::ModelTypeSet registered_types,
                                      syncer::ModelTypeSet preferred_types) {
  DCHECK(CalledOnValidThread());
  DCHECK(registered_types.HasAll(preferred_types));
  preferred_types = ResolvePrefGroups(registered_types, preferred_types);
  for (syncer::ModelTypeSet::Iterator i = registered_types.First(); i.Good();
       i.Inc()) {
    SetDataTypePreferred(i.Get(), preferred_types.Has(i.Get()));
  }
}

bool SyncPrefs::IsManaged() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncManaged);
}

std::string SyncPrefs::GetEncryptionBootstrapToken() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetString(prefs::kSyncEncryptionBootstrapToken);
}

void SyncPrefs::SetEncryptionBootstrapToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncEncryptionBootstrapToken, token);
}

std::string SyncPrefs::GetKeystoreEncryptionBootstrapToken() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetString(prefs::kSyncKeystoreEncryptionBootstrapToken);
}

void SyncPrefs::SetKeystoreEncryptionBootstrapToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncKeystoreEncryptionBootstrapToken, token);
}

std::string SyncPrefs::GetSyncSessionsGUID() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetString(prefs::kSyncSessionsGUID);
}

void SyncPrefs::SetSyncSessionsGUID(const std::string& guid) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncSessionsGUID, guid);
}

// static
const char* SyncPrefs::GetPrefNameForDataType(syncer::ModelType data_type) {
  switch (data_type) {
    case syncer::BOOKMARKS:
      return prefs::kSyncBookmarks;
    case syncer::PASSWORDS:
      return prefs::kSyncPasswords;
    case syncer::PREFERENCES:
      return prefs::kSyncPreferences;
    case syncer::AUTOFILL:
      return prefs::kSyncAutofill;
    case syncer::AUTOFILL_PROFILE:
      return prefs::kSyncAutofillProfile;
    case syncer::THEMES:
      return prefs::kSyncThemes;
    case syncer::TYPED_URLS:
      return prefs::kSyncTypedUrls;
    case syncer::EXTENSION_SETTINGS:
      return prefs::kSyncExtensionSettings;
    case syncer::EXTENSIONS:
      return prefs::kSyncExtensions;
    case syncer::APP_LIST:
      return prefs::kSyncAppList;
    case syncer::APP_SETTINGS:
      return prefs::kSyncAppSettings;
    case syncer::APPS:
      return prefs::kSyncApps;
    case syncer::SEARCH_ENGINES:
      return prefs::kSyncSearchEngines;
    case syncer::SESSIONS:
      return prefs::kSyncSessions;
    case syncer::APP_NOTIFICATIONS:
      return prefs::kSyncAppNotifications;
    case syncer::HISTORY_DELETE_DIRECTIVES:
      return prefs::kSyncHistoryDeleteDirectives;
    case syncer::SYNCED_NOTIFICATIONS:
      return prefs::kSyncSyncedNotifications;
    case syncer::SYNCED_NOTIFICATION_APP_INFO:
      return prefs::kSyncSyncedNotificationAppInfo;
    case syncer::DICTIONARY:
      return prefs::kSyncDictionary;
    case syncer::FAVICON_IMAGES:
      return prefs::kSyncFaviconImages;
    case syncer::FAVICON_TRACKING:
      return prefs::kSyncFaviconTracking;
    case syncer::SUPERVISED_USER_SETTINGS:
      return prefs::kSyncSupervisedUserSettings;
    case syncer::PROXY_TABS:
      return prefs::kSyncTabs;
    case syncer::PRIORITY_PREFERENCES:
      return prefs::kSyncPriorityPreferences;
    case syncer::SUPERVISED_USERS:
      return prefs::kSyncSupervisedUsers;
    case syncer::ARTICLES:
      return prefs::kSyncArticles;
    case syncer::SUPERVISED_USER_SHARED_SETTINGS:
      return prefs::kSyncSupervisedUserSharedSettings;
    case syncer::DEVICE_INFO:
      return prefs::kSyncDeviceInfo;
    default:
      break;
  }
  NOTREACHED();
  return NULL;
}

#if defined(OS_CHROMEOS)
std::string SyncPrefs::GetSpareBootstrapToken() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetString(prefs::kSyncSpareBootstrapToken);
}

void SyncPrefs::SetSpareBootstrapToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncSpareBootstrapToken, token);
}
#endif

void SyncPrefs::AcknowledgeSyncedTypes(syncer::ModelTypeSet types) {
  DCHECK(CalledOnValidThread());
  // Add the types to the current set of acknowledged
  // types, and then store the resulting set in prefs.
  const syncer::ModelTypeSet acknowledged_types =
      Union(types,
            syncer::ModelTypeSetFromValue(
                *pref_service_->GetList(prefs::kSyncAcknowledgedSyncTypes)));

  scoped_ptr<base::ListValue> value(
      syncer::ModelTypeSetToValue(acknowledged_types));
  pref_service_->Set(prefs::kSyncAcknowledgedSyncTypes, *value);
}

int SyncPrefs::GetRemainingRollbackTries() const {
  return pref_service_->GetInteger(prefs::kSyncRemainingRollbackTries);
}

void SyncPrefs::SetRemainingRollbackTries(int times) {
  pref_service_->SetInteger(prefs::kSyncRemainingRollbackTries, times);
}

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(SyncPrefObserver,
                    sync_pref_observers_,
                    OnSyncManagedPrefChange(*pref_sync_managed_));
}

void SyncPrefs::SetManagedForTest(bool is_managed) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncManaged, is_managed);
}

syncer::ModelTypeSet SyncPrefs::GetAcknowledgeSyncedTypesForTest() const {
  DCHECK(CalledOnValidThread());
  return syncer::ModelTypeSetFromValue(
      *pref_service_->GetList(prefs::kSyncAcknowledgedSyncTypes));
}

void SyncPrefs::RegisterPrefGroups() {
  pref_groups_[syncer::APPS].Put(syncer::APP_NOTIFICATIONS);
  pref_groups_[syncer::APPS].Put(syncer::APP_SETTINGS);
  pref_groups_[syncer::APPS].Put(syncer::APP_LIST);

  pref_groups_[syncer::AUTOFILL].Put(syncer::AUTOFILL_PROFILE);

  pref_groups_[syncer::EXTENSIONS].Put(syncer::EXTENSION_SETTINGS);

  pref_groups_[syncer::PREFERENCES].Put(syncer::DICTIONARY);
  pref_groups_[syncer::PREFERENCES].Put(syncer::PRIORITY_PREFERENCES);
  pref_groups_[syncer::PREFERENCES].Put(syncer::SEARCH_ENGINES);

  pref_groups_[syncer::TYPED_URLS].Put(syncer::HISTORY_DELETE_DIRECTIVES);
  pref_groups_[syncer::TYPED_URLS].Put(syncer::SESSIONS);
  pref_groups_[syncer::TYPED_URLS].Put(syncer::FAVICON_IMAGES);
  pref_groups_[syncer::TYPED_URLS].Put(syncer::FAVICON_TRACKING);

  pref_groups_[syncer::PROXY_TABS].Put(syncer::SESSIONS);
  pref_groups_[syncer::PROXY_TABS].Put(syncer::FAVICON_IMAGES);
  pref_groups_[syncer::PROXY_TABS].Put(syncer::FAVICON_TRACKING);

  // TODO(zea): put favicons in the bookmarks group as well once it handles
  // those favicons.
}

// static
void SyncPrefs::RegisterDataTypePreferredPref(
    user_prefs::PrefRegistrySyncable* registry,
    syncer::ModelType type,
    bool is_preferred) {
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return;
  }
  registry->RegisterBooleanPref(
      pref_name,
      is_preferred,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool SyncPrefs::GetDataTypePreferred(syncer::ModelType type) const {
  DCHECK(CalledOnValidThread());
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return false;
  }
  if (type == syncer::PROXY_TABS &&
      pref_service_->GetUserPrefValue(pref_name) == NULL &&
      pref_service_->IsUserModifiablePreference(pref_name)) {
    // If there is no tab sync preference yet (i.e. newly enabled type),
    // default to the session sync preference value.
    pref_name = GetPrefNameForDataType(syncer::SESSIONS);
  }

  return pref_service_->GetBoolean(pref_name);
}

void SyncPrefs::SetDataTypePreferred(syncer::ModelType type,
                                     bool is_preferred) {
  DCHECK(CalledOnValidThread());
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return;
  }
  pref_service_->SetBoolean(pref_name, is_preferred);
}

syncer::ModelTypeSet SyncPrefs::ResolvePrefGroups(
    syncer::ModelTypeSet registered_types,
    syncer::ModelTypeSet types) const {
  DCHECK(registered_types.HasAll(types));
  syncer::ModelTypeSet types_with_groups = types;
  for (PrefGroupsMap::const_iterator i = pref_groups_.begin();
       i != pref_groups_.end();
       ++i) {
    if (types.Has(i->first))
      types_with_groups.PutAll(i->second);
  }
  types_with_groups.RetainAll(registered_types);
  return types_with_groups;
}

base::Time SyncPrefs::GetFirstSyncTime() const {
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncFirstSyncTime));
}

void SyncPrefs::SetFirstSyncTime(base::Time time) {
  pref_service_->SetInt64(prefs::kSyncFirstSyncTime, time.ToInternalValue());
}

void SyncPrefs::ClearFirstSyncTime() {
  pref_service_->ClearPref(prefs::kSyncFirstSyncTime);
}

}  // namespace sync_driver
