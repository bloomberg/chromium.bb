// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pref_registry/pref_registry_syncable.h"

#include "base/files/file_path.h"
#include "base/prefs/default_pref_store.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace user_prefs {

PrefRegistrySyncable::PrefRegistrySyncable() {
}

PrefRegistrySyncable::~PrefRegistrySyncable() {
}

void PrefRegistrySyncable::SetSyncableRegistrationCallback(
    const SyncableRegistrationCallback& cb) {
  callback_ = cb;
}

void PrefRegistrySyncable::RegisterBooleanPref(const char* path,
                                               bool default_value,
                                               uint32 flags) {
  RegisterSyncablePreference(
      path, new base::FundamentalValue(default_value), flags);
}

void PrefRegistrySyncable::RegisterIntegerPref(const char* path,
                                               int default_value,
                                               uint32 flags) {
  RegisterSyncablePreference(
      path, new base::FundamentalValue(default_value), flags);
}

void PrefRegistrySyncable::RegisterDoublePref(const char* path,
                                              double default_value,
                                              uint32 flags) {
  RegisterSyncablePreference(
      path, new base::FundamentalValue(default_value), flags);
}

void PrefRegistrySyncable::RegisterStringPref(const char* path,
                                              const std::string& default_value,
                                              uint32 flags) {
  RegisterSyncablePreference(
      path, new base::StringValue(default_value), flags);
}

void PrefRegistrySyncable::RegisterFilePathPref(
    const char* path,
    const base::FilePath& default_value,
    uint32 flags) {
  RegisterSyncablePreference(
      path, new base::StringValue(default_value.value()), flags);
}

void PrefRegistrySyncable::RegisterListPref(const char* path, uint32 flags) {
  RegisterSyncablePreference(path, new base::ListValue(), flags);
}

void PrefRegistrySyncable::RegisterListPref(const char* path,
                                            base::ListValue* default_value,
                                            uint32 flags) {
  RegisterSyncablePreference(path, default_value, flags);
}

void PrefRegistrySyncable::RegisterDictionaryPref(const char* path,
                                                  uint32 flags) {
  RegisterSyncablePreference(path, new base::DictionaryValue(), flags);
}

void PrefRegistrySyncable::RegisterDictionaryPref(
    const char* path,
    base::DictionaryValue* default_value,
    uint32 flags) {
  RegisterSyncablePreference(path, default_value, flags);
}

void PrefRegistrySyncable::RegisterInt64Pref(
    const char* path,
    int64 default_value,
    uint32 flags) {
  RegisterSyncablePreference(
      path,
      new base::StringValue(base::Int64ToString(default_value)),
      flags);
}

void PrefRegistrySyncable::RegisterUint64Pref(
    const char* path,
    uint64 default_value,
    uint32 flags) {
  RegisterSyncablePreference(
      path,
      new base::StringValue(base::Uint64ToString(default_value)),
      flags);
}

void PrefRegistrySyncable::RegisterSyncablePreference(
    const char* path,
    base::Value* default_value,
    uint32 flags) {
  // Tests that |flags| does not contain both SYNCABLE_PREF and
  // SYNCABLE_PRIORITY_PREF flags at the same time.
  DCHECK(!(flags & PrefRegistrySyncable::SYNCABLE_PREF) ||
         !(flags & PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF));
  PrefRegistry::RegisterPreference(path, default_value, flags);

  if (flags & PrefRegistrySyncable::SYNCABLE_PREF ||
      flags & PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF) {
    if (!callback_.is_null())
      callback_.Run(path, flags);
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
