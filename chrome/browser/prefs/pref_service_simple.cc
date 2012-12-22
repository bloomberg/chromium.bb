// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service.h"

#include "base/file_path.h"
#include "base/string_number_conversions.h"
#include "base/values.h"

PrefServiceSimple::PrefServiceSimple(
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
}

PrefServiceSimple::~PrefServiceSimple() {}

void PrefServiceSimple::RegisterBooleanPref(const char* path,
                                            bool default_value) {
  RegisterPreference(path, Value::CreateBooleanValue(default_value));
}

void PrefServiceSimple::RegisterIntegerPref(const char* path,
                                            int default_value) {
  RegisterPreference(path, Value::CreateIntegerValue(default_value));
}

void PrefServiceSimple::RegisterDoublePref(const char* path,
                                           double default_value) {
  RegisterPreference(path, Value::CreateDoubleValue(default_value));
}

void PrefServiceSimple::RegisterStringPref(const char* path,
                                           const std::string& default_value) {
  RegisterPreference(path, Value::CreateStringValue(default_value));
}

void PrefServiceSimple::RegisterFilePathPref(const char* path,
                                             const FilePath& default_value) {
  RegisterPreference(path, Value::CreateStringValue(default_value.value()));
}

void PrefServiceSimple::RegisterListPref(const char* path) {
  RegisterPreference(path, new ListValue());
}

void PrefServiceSimple::RegisterListPref(const char* path,
                                         ListValue* default_value) {
  RegisterPreference(path, default_value);
}

void PrefServiceSimple::RegisterDictionaryPref(const char* path) {
  RegisterPreference(path, new DictionaryValue());
}

void PrefServiceSimple::RegisterDictionaryPref(const char* path,
                                               DictionaryValue* default_value) {
  RegisterPreference(path, default_value);
}

void PrefServiceSimple::RegisterInt64Pref(const char* path,
                                          int64 default_value) {
  RegisterPreference(
      path, Value::CreateStringValue(base::Int64ToString(default_value)));
}
