// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pref_registry/pref_registry_syncable.h"

#include "base/files/file_path.h"
#include "base/prefs/default_pref_store.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_prefs {

namespace {

// A helper function for RegisterLocalized*Pref that creates a Value*
// based on a localized resource.  Because we control the values in a
// locale dll, this should always return a Value of the appropriate
// type.
base::Value* CreateLocaleDefaultValue(base::Value::Type type,
                                      int message_id) {
  const std::string resource_string = l10n_util::GetStringUTF8(message_id);
  DCHECK(!resource_string.empty());
  switch (type) {
    case base::Value::TYPE_BOOLEAN: {
      if ("true" == resource_string)
        return new base::FundamentalValue(true);
      if ("false" == resource_string)
        return new base::FundamentalValue(false);
      break;
    }

    case base::Value::TYPE_INTEGER: {
      int val;
      base::StringToInt(resource_string, &val);
      return new base::FundamentalValue(val);
    }

    case base::Value::TYPE_DOUBLE: {
      double val;
      base::StringToDouble(resource_string, &val);
      return new base::FundamentalValue(val);
    }

    case base::Value::TYPE_STRING: {
      return new base::StringValue(resource_string);
    }

    default: {
      NOTREACHED() <<
          "list and dictionary types cannot have default locale values";
    }
  }
  NOTREACHED();
  return base::Value::CreateNullValue();
}

}  // namespace

PrefRegistrySyncable::PrefRegistrySyncable() {
}

PrefRegistrySyncable::~PrefRegistrySyncable() {
}

const PrefRegistrySyncable::PrefToStatus&
PrefRegistrySyncable::syncable_preferences() const {
  return syncable_preferences_;
}

void PrefRegistrySyncable::SetSyncableRegistrationCallback(
    const SyncableRegistrationCallback& cb) {
  callback_ = cb;
}

void PrefRegistrySyncable::RegisterBooleanPref(const char* path,
                                               bool default_value,
                                               PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path, new base::FundamentalValue(default_value), sync_status);
}

void PrefRegistrySyncable::RegisterIntegerPref(const char* path,
                                               int default_value,
                                               PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path, new base::FundamentalValue(default_value), sync_status);
}

void PrefRegistrySyncable::RegisterDoublePref(const char* path,
                                              double default_value,
                                              PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path, new base::FundamentalValue(default_value), sync_status);
}

void PrefRegistrySyncable::RegisterStringPref(const char* path,
                                              const std::string& default_value,
                                              PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path, new base::StringValue(default_value), sync_status);
}

void PrefRegistrySyncable::RegisterFilePathPref(
    const char* path,
    const base::FilePath& default_value,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path, new base::StringValue(default_value.value()), sync_status);
}

void PrefRegistrySyncable::RegisterListPref(const char* path,
                                            PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path, new base::ListValue(), sync_status);
}

void PrefRegistrySyncable::RegisterListPref(const char* path,
                                            base::ListValue* default_value,
                                            PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path, default_value, sync_status);
}

void PrefRegistrySyncable::RegisterDictionaryPref(const char* path,
                                                  PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path, new base::DictionaryValue(), sync_status);
}

void PrefRegistrySyncable::RegisterDictionaryPref(
    const char* path,
    base::DictionaryValue* default_value,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(path, default_value, sync_status);
}

void PrefRegistrySyncable::RegisterLocalizedBooleanPref(
    const char* path,
    int locale_default_message_id,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      CreateLocaleDefaultValue(base::Value::TYPE_BOOLEAN,
                               locale_default_message_id),
      sync_status);
}

void PrefRegistrySyncable::RegisterLocalizedIntegerPref(
    const char* path,
    int locale_default_message_id,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      CreateLocaleDefaultValue(base::Value::TYPE_INTEGER,
                               locale_default_message_id),
      sync_status);
}

void PrefRegistrySyncable::RegisterLocalizedDoublePref(
    const char* path,
    int locale_default_message_id,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      CreateLocaleDefaultValue(base::Value::TYPE_DOUBLE,
                               locale_default_message_id),
      sync_status);
}

void PrefRegistrySyncable::RegisterLocalizedStringPref(
    const char* path,
    int locale_default_message_id,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      CreateLocaleDefaultValue(base::Value::TYPE_STRING,
                               locale_default_message_id),
      sync_status);
}

void PrefRegistrySyncable::RegisterInt64Pref(
    const char* path,
    int64 default_value,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      new base::StringValue(base::Int64ToString(default_value)),
      sync_status);
}

void PrefRegistrySyncable::RegisterUint64Pref(
    const char* path,
    uint64 default_value,
    PrefSyncStatus sync_status) {
  RegisterSyncablePreference(
      path,
      new base::StringValue(base::Uint64ToString(default_value)),
      sync_status);
}

void PrefRegistrySyncable::RegisterSyncablePreference(
    const char* path,
    base::Value* default_value,
    PrefSyncStatus sync_status) {
  PrefRegistry::RegisterPreference(path, default_value);

  if (sync_status == PrefRegistrySyncable::SYNCABLE_PREF ||
      sync_status == PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF) {
    syncable_preferences_[path] = sync_status;

    if (!callback_.is_null())
      callback_.Run(path, sync_status);
  }
}

scoped_refptr<PrefRegistrySyncable> PrefRegistrySyncable::ForkForIncognito() {
  // TODO(joi): We can directly reuse the same PrefRegistry once
  // PrefService no longer registers for callbacks on registration and
  // unregistration.
  scoped_refptr<PrefRegistrySyncable> registry(new PrefRegistrySyncable());
  registry->defaults_ = defaults_;
  return registry;
}

}  // namespace user_prefs
