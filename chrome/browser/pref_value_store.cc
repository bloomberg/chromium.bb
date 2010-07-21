// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pref_value_store.h"

PrefValueStore::PrefValueStore(PrefStore* managed_prefs,
                               PrefStore* extension_prefs,
                               PrefStore* command_line_prefs,
                               PrefStore* user_prefs,
                               PrefStore* recommended_prefs) {
  pref_stores_[MANAGED].reset(managed_prefs);
  pref_stores_[EXTENSION].reset(extension_prefs);
  pref_stores_[COMMAND_LINE].reset(command_line_prefs);
  pref_stores_[USER].reset(user_prefs);
  pref_stores_[RECOMMENDED].reset(recommended_prefs);
}

PrefValueStore::~PrefValueStore() { }

bool PrefValueStore::GetValue(const std::wstring& name,
                              Value** out_value) const {
  // Check the |PrefStore|s in order of their priority from highest to lowest
  // to find the value of the preference described by the given preference name.
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get() &&
        pref_stores_[i]->prefs()->Get(name.c_str(), out_value)) {
      return true;
    }
  }
  // No value found for the given preference name, set the return false.
  *out_value = NULL;
  return false;
}

bool PrefValueStore::WritePrefs() {
  bool success = true;
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get())
      success = pref_stores_[i]->WritePrefs() && success;
  }
  return success;
}

void PrefValueStore::ScheduleWritePrefs() {
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get())
      pref_stores_[i]->ScheduleWritePrefs();
  }
}

PrefStore::PrefReadError PrefValueStore::ReadPrefs() {
  PrefStore::PrefReadError result = PrefStore::PREF_READ_ERROR_NONE;
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get()) {
      PrefStore::PrefReadError this_error = pref_stores_[i]->ReadPrefs();
      if (result == PrefStore::PREF_READ_ERROR_NONE)
        result = this_error;
    }
  }
  // TODO(markusheintz): Return a better error status: maybe a struct with
  // the error status of all PrefStores.
  return result;
}

bool PrefValueStore::HasPrefPath(const wchar_t* path) const {
  Value* tmp_value = NULL;
  const std::wstring name(path);
  bool rv = GetValue(name, &tmp_value);
  return rv;
}

// Note the |DictionaryValue| referenced by the |PrefStore| user_prefs_
// (returned by the method prefs()) takes the ownership of the Value referenced
// by in_value.
void PrefValueStore::SetUserPrefValue(const wchar_t* name, Value* in_value) {
  pref_stores_[USER]->prefs()->Set(name, in_value);
}

bool PrefValueStore::ReadOnly() {
  return pref_stores_[USER]->ReadOnly();
}

void PrefValueStore::RemoveUserPrefValue(const wchar_t* name) {
  if (pref_stores_[USER].get()) {
    pref_stores_[USER]->prefs()->Remove(name, NULL);
  }
}

bool PrefValueStore::PrefValueInManagedStore(const wchar_t* name) {
  return PrefValueInStore(name, MANAGED);
}

bool PrefValueStore::PrefValueInExtensionStore(const wchar_t* name) {
  return PrefValueInStore(name, EXTENSION);
}

bool PrefValueStore::PrefValueInUserStore(const wchar_t* name) {
  return PrefValueInStore(name, USER);
}

bool PrefValueStore::PrefValueFromExtensionStore(const wchar_t* name) {
  return ControllingPrefStoreForPref(name) == EXTENSION;
}

bool PrefValueStore::PrefValueFromUserStore(const wchar_t* name) {
  return ControllingPrefStoreForPref(name) == USER;
}

bool PrefValueStore::PrefValueUserModifiable(const wchar_t* name) {
  PrefStoreType effective_store = ControllingPrefStoreForPref(name);
  return effective_store >= USER || effective_store == INVALID;
}

bool PrefValueStore::PrefValueInStore(const wchar_t* name, PrefStoreType type) {
  if (pref_stores_[type].get() == NULL) {
    // No store of that type set, so this pref can't be in it.
    return false;
  }
  Value* tmp_value;
  return pref_stores_[type]->prefs()->Get(name, &tmp_value);
}

PrefValueStore::PrefStoreType PrefValueStore::ControllingPrefStoreForPref(
    const wchar_t* name) {
  for (int i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (PrefValueInStore(name, static_cast<PrefStoreType>(i)))
      return static_cast<PrefStoreType>(i);
  }
  return INVALID;
}
