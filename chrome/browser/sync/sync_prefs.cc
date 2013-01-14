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

SyncPrefs::SyncPrefs(PrefServiceSyncable* pref_service)
    : pref_service_(pref_service) {
  RegisterPrefGroups();
  // TODO(tim): Create a Mock instead of maintaining the if(!pref_service_) case
  // throughout this file.  This is a problem now due to lack of injection at
  // ProfileSyncService. Bug 130176.
  if (pref_service_) {
    // Watch the preference that indicates sync is managed so we can take
    // appropriate action.
    pref_sync_managed_.Init(prefs::kSyncManaged, pref_service_,
                            base::Bind(&SyncPrefs::OnSyncManagedPrefChanged,
                                       base::Unretained(this)));
  }
}

SyncPrefs::~SyncPrefs() {
  DCHECK(CalledOnValidThread());
}

// static
void SyncPrefs::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kSyncHasSetupCompleted,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSyncSuppressStart,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterInt64Pref(prefs::kSyncLastSyncedTime,
                           0,
                           PrefServiceSyncable::UNSYNCABLE_PREF);

  // If you've never synced before, or if you're using Chrome OS or Android,
  // all datatypes are on by default.
  // TODO(nick): Perhaps a better model would be to always default to false,
  // and explicitly call SetDataTypes() when the user shows the wizard.
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  bool enable_by_default = true;
#else
  bool enable_by_default = !prefs->HasPrefPath(prefs::kSyncHasSetupCompleted);
#endif

  prefs->RegisterBooleanPref(prefs::kSyncKeepEverythingSynced,
                             enable_by_default,
                             PrefServiceSyncable::UNSYNCABLE_PREF);

  syncer::ModelTypeSet user_types = syncer::UserTypes();

  // Treat bookmarks specially.
  RegisterDataTypePreferredPref(prefs, syncer::BOOKMARKS, true);
  user_types.Remove(syncer::BOOKMARKS);

  for (syncer::ModelTypeSet::Iterator it = user_types.First();
       it.Good(); it.Inc()) {
    RegisterDataTypePreferredPref(prefs, it.Get(), enable_by_default);
  }

  prefs->RegisterBooleanPref(prefs::kSyncManaged,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kSyncEncryptionBootstrapToken,
                            "",
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kSyncKeystoreEncryptionBootstrapToken,
                            "",
                            PrefServiceSyncable::UNSYNCABLE_PREF);
#if defined(OS_CHROMEOS)
  prefs->RegisterStringPref(prefs::kSyncSpareBootstrapToken,
                            "",
                            PrefServiceSyncable::UNSYNCABLE_PREF);
#endif

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
  model_set.Put(syncer::TYPED_URLS);
  model_set.Put(syncer::SESSIONS);
  prefs->RegisterListPref(prefs::kSyncAcknowledgedSyncTypes,
                          syncer::ModelTypeSetToValue(model_set),
                          PrefServiceSyncable::UNSYNCABLE_PREF);
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
  pref_service_->ClearPref(prefs::kSyncKeystoreEncryptionBootstrapToken);

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

// TODO(akalin): If encryption is turned on for all data types,
// history delete directives are useless and so we shouldn't bother
// enabling them.

syncer::ModelTypeSet SyncPrefs::GetPreferredDataTypes(
    syncer::ModelTypeSet registered_types) const {
  DCHECK(CalledOnValidThread());
  if (!pref_service_) {
    return syncer::ModelTypeSet();
  }

  // First remove any datatypes that are inconsistent with the current policies
  // on the client (so that "keep everything synced" doesn't include them).
  if (pref_service_->HasPrefPath(prefs::kSavingBrowserHistoryDisabled) &&
      pref_service_->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    registered_types.Remove(syncer::TYPED_URLS);
  }

  if (pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced)) {
    return registered_types;
  }

  syncer::ModelTypeSet preferred_types;
  for (syncer::ModelTypeSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    if (GetDataTypePreferred(it.Get())) {
      preferred_types.Put(it.Get());
    }
  }
  return ResolvePrefGroups(registered_types, preferred_types);
}

void SyncPrefs::SetPreferredDataTypes(
    syncer::ModelTypeSet registered_types,
    syncer::ModelTypeSet preferred_types) {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  DCHECK(registered_types.HasAll(preferred_types));
  preferred_types = ResolvePrefGroups(registered_types, preferred_types);
  for (syncer::ModelTypeSet::Iterator i = registered_types.First();
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

std::string SyncPrefs::GetKeystoreEncryptionBootstrapToken() const {
  DCHECK(CalledOnValidThread());
  return
      pref_service_ ?
      pref_service_->GetString(prefs::kSyncKeystoreEncryptionBootstrapToken) :
      "";
}

void SyncPrefs::SetKeystoreEncryptionBootstrapToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncKeystoreEncryptionBootstrapToken, token);
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
    default:
      break;
  }
  NOTREACHED();
  return NULL;
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

void SyncPrefs::AcknowledgeSyncedTypes(syncer::ModelTypeSet types) {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  // Add the types to the current set of acknowledged
  // types, and then store the resulting set in prefs.
  const syncer::ModelTypeSet acknowledged_types =
      Union(types,
            syncer::ModelTypeSetFromValue(
                *pref_service_->GetList(prefs::kSyncAcknowledgedSyncTypes)));

  scoped_ptr<ListValue> value(
      syncer::ModelTypeSetToValue(acknowledged_types));
  pref_service_->Set(prefs::kSyncAcknowledgedSyncTypes, *value);
}

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(SyncPrefObserver, sync_pref_observers_,
                    OnSyncManagedPrefChange(*pref_sync_managed_));
}

void SyncPrefs::SetManagedForTest(bool is_managed) {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kSyncManaged, is_managed);
}

syncer::ModelTypeSet SyncPrefs::GetAcknowledgeSyncedTypesForTest() const {
  DCHECK(CalledOnValidThread());
  if (!pref_service_) {
    return syncer::ModelTypeSet();
  }
  return syncer::ModelTypeSetFromValue(
      *pref_service_->GetList(prefs::kSyncAcknowledgedSyncTypes));
}

void SyncPrefs::RegisterPrefGroups() {
  pref_groups_[syncer::APPS].Put(syncer::APP_NOTIFICATIONS);
  pref_groups_[syncer::APPS].Put(syncer::APP_SETTINGS);

  pref_groups_[syncer::AUTOFILL].Put(syncer::AUTOFILL_PROFILE);

  pref_groups_[syncer::EXTENSIONS].Put(syncer::EXTENSION_SETTINGS);

  pref_groups_[syncer::PREFERENCES].Put(syncer::SEARCH_ENGINES);

  // TODO(akalin): Revisit this once UI lands.
  pref_groups_[syncer::SESSIONS].Put(syncer::HISTORY_DELETE_DIRECTIVES);
}

// static
void SyncPrefs::RegisterDataTypePreferredPref(PrefServiceSyncable* prefs,
                                              syncer::ModelType type,
                                              bool is_preferred) {
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return;
  }
  prefs->RegisterBooleanPref(pref_name, is_preferred,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
}

bool SyncPrefs::GetDataTypePreferred(syncer::ModelType type) const {
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
    syncer::ModelType type, bool is_preferred) {
  DCHECK(CalledOnValidThread());
  CHECK(pref_service_);
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
