// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_syncable.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/overlay_user_pref_store.h"
#include "base/string_number_conversions.h"
#include "base/value_conversions.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/pref_notifier_impl.h"
#include "chrome/browser/prefs/pref_service_syncable_observer.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// A helper function for RegisterLocalized*Pref that creates a Value*
// based on a localized resource.  Because we control the values in a
// locale dll, this should always return a Value of the appropriate
// type.
Value* CreateLocaleDefaultValue(base::Value::Type type,
                                int message_id) {
  const std::string& resource_string = l10n_util::GetStringUTF8(message_id);
  DCHECK(!resource_string.empty());
  switch (type) {
    case Value::TYPE_BOOLEAN: {
      if ("true" == resource_string)
        return Value::CreateBooleanValue(true);
      if ("false" == resource_string)
        return Value::CreateBooleanValue(false);
      break;
    }

    case Value::TYPE_INTEGER: {
      int val;
      base::StringToInt(resource_string, &val);
      return Value::CreateIntegerValue(val);
    }

    case Value::TYPE_DOUBLE: {
      double val;
      base::StringToDouble(resource_string, &val);
      return Value::CreateDoubleValue(val);
    }

    case Value::TYPE_STRING: {
      return Value::CreateStringValue(resource_string);
    }

    default: {
      NOTREACHED() <<
          "list and dictionary types cannot have default locale values";
    }
  }
  NOTREACHED();
  return Value::CreateNullValue();
}

}  // namespace

PrefServiceSyncable::PrefServiceSyncable(
    PrefNotifierImpl* pref_notifier,
    PrefValueStore* pref_value_store,
    PersistentPrefStore* user_prefs,
    DefaultPrefStore* default_store,
    base::Callback<void(PersistentPrefStore::PrefReadError)>
        read_error_callback,
    bool async)
  : PrefService(pref_notifier,
                pref_value_store,
                user_prefs,
                default_store,
                read_error_callback,
                async) {
  pref_sync_associator_.SetPrefService(this);

  pref_value_store->set_callback(
      base::Bind(&PrefModelAssociator::ProcessPrefChange,
                 base::Unretained(&pref_sync_associator_)));
}

PrefServiceSyncable::~PrefServiceSyncable() {}

PrefServiceSyncable* PrefServiceSyncable::CreateIncognitoPrefService(
    PrefStore* incognito_extension_prefs) {
  pref_service_forked_ = true;
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  OverlayUserPrefStore* incognito_pref_store =
      new OverlayUserPrefStore(user_pref_store_.get());
  PrefsTabHelper::InitIncognitoUserPrefStore(incognito_pref_store);
  PrefServiceSyncable* incognito_service = new PrefServiceSyncable(
      pref_notifier,
      pref_value_store_->CloneAndSpecialize(
          NULL,  // managed
          incognito_extension_prefs,
          NULL,  // command_line_prefs
          incognito_pref_store,
          NULL,  // recommended
          default_store_.get(),
          pref_notifier),
      incognito_pref_store,
      default_store_.get(),
      read_error_callback_,
      false);
  return incognito_service;
}

bool PrefServiceSyncable::IsSyncing() {
  return pref_sync_associator_.models_associated();
}

void PrefServiceSyncable::AddObserver(PrefServiceSyncableObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PrefServiceSyncable::RemoveObserver(
    PrefServiceSyncableObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void PrefServiceSyncable::UnregisterPreference(const char* path) {
  PrefService::UnregisterPreference(path);
  if (pref_sync_associator_.IsPrefRegistered(path)) {
    pref_sync_associator_.UnregisterPref(path);
  }
}

void PrefServiceSyncable::RegisterBooleanPref(const char* path,
                                              bool default_value,
                                              PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path,
                             Value::CreateBooleanValue(default_value),
                             sync_status);
}

void PrefServiceSyncable::RegisterIntegerPref(const char* path,
                                              int default_value,
                                              PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path,
                             Value::CreateIntegerValue(default_value),
                             sync_status);
}

void PrefServiceSyncable::RegisterDoublePref(const char* path,
                                             double default_value,
                                             PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path,
                             Value::CreateDoubleValue(default_value),
                             sync_status);
}

void PrefServiceSyncable::RegisterStringPref(const char* path,
                                             const std::string& default_value,
                                             PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path,
                             Value::CreateStringValue(default_value),
                             sync_status);
}

void PrefServiceSyncable::RegisterFilePathPref(const char* path,
                                               const FilePath& default_value,
                                               PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path,
                             Value::CreateStringValue(default_value.value()),
                             sync_status);
}

void PrefServiceSyncable::RegisterListPref(const char* path,
                                           PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path, new ListValue(), sync_status);
}

void PrefServiceSyncable::RegisterListPref(const char* path,
                                           ListValue* default_value,
                                           PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path, default_value, sync_status);
}

void PrefServiceSyncable::RegisterDictionaryPref(const char* path,
                                                 PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path, new DictionaryValue(), sync_status);
}

void PrefServiceSyncable::RegisterDictionaryPref(const char* path,
                                                 DictionaryValue* default_value,
                                                 PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path, default_value, sync_status);
}

void PrefServiceSyncable::RegisterLocalizedBooleanPref(
    const char* path,
    int locale_default_message_id,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      CreateLocaleDefaultValue(Value::TYPE_BOOLEAN, locale_default_message_id),
      sync_status);
}

void PrefServiceSyncable::RegisterLocalizedIntegerPref(
    const char* path,
    int locale_default_message_id,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      CreateLocaleDefaultValue(Value::TYPE_INTEGER, locale_default_message_id),
      sync_status);
}

void PrefServiceSyncable::RegisterLocalizedDoublePref(
    const char* path,
    int locale_default_message_id,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      CreateLocaleDefaultValue(Value::TYPE_DOUBLE, locale_default_message_id),
      sync_status);
}

void PrefServiceSyncable::RegisterLocalizedStringPref(
    const char* path,
    int locale_default_message_id,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      CreateLocaleDefaultValue(Value::TYPE_STRING, locale_default_message_id),
      sync_status);
}

void PrefServiceSyncable::RegisterInt64Pref(
    const char* path,
    int64 default_value,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      Value::CreateStringValue(base::Int64ToString(default_value)),
      sync_status);
}

void PrefServiceSyncable::RegisterUint64Pref(
    const char* path,
    uint64 default_value,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      Value::CreateStringValue(base::Uint64ToString(default_value)),
      sync_status);
}

syncer::SyncableService* PrefServiceSyncable::GetSyncableService() {
  return &pref_sync_associator_;
}

void PrefServiceSyncable::UpdateCommandLinePrefStore(
    PrefStore* cmd_line_store) {
  // If |pref_service_forked_| is true, then this PrefService and the forked
  // copies will be out of sync.
  DCHECK(!pref_service_forked_);
  PrefService::UpdateCommandLinePrefStore(cmd_line_store);
}

void PrefServiceSyncable::OnIsSyncingChanged() {
  FOR_EACH_OBSERVER(PrefServiceSyncableObserver, observer_list_,
                    OnIsSyncingChanged());
}

void PrefServiceSyncable::RegisterSyncablePreference(
    const char* path, Value* default_value, PrefSyncStatus sync_status) {
  PrefService::RegisterPreference(path, default_value);
  // Register with sync if necessary.
  if (sync_status == SYNCABLE_PREF)
    pref_sync_associator_.RegisterPref(path);
}
