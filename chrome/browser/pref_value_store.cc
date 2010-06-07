// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pref_value_store.h"

PrefValueStore::PrefValueStore(PrefStore* managed_prefs,
                               PrefStore* user_prefs,
                               PrefStore* recommended_prefs)
    : managed_prefs_(managed_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {
}

PrefValueStore::~PrefValueStore() { }

bool PrefValueStore::GetValue(
    const std::wstring& name, Value** out_value) const {
  // Check the |PrefStore|s in order of their priority from highest to lowest
  // to find the value of the preference described by the given preference name.
  if (managed_prefs_.get() &&
      managed_prefs_->prefs()->Get(name.c_str(), out_value) ) {
    return true;
  } else if (user_prefs_.get() &&
             user_prefs_->prefs()->Get(name.c_str(), out_value) ) {
    return true;
  } else if (recommended_prefs_.get() &&
             recommended_prefs_->prefs()->Get(name.c_str(), out_value)) {
    return true;
  }
  // No value found for the given preference name, set the return false.
  *out_value = NULL;
  return false;
}

bool PrefValueStore::WritePrefs() {
  // Managed and recommended preferences are not set by the user.
  // Hence they will not be written out.
  return user_prefs_->WritePrefs();
}

void PrefValueStore::ScheduleWritePrefs() {
  // Managed and recommended preferences are not set by the user.
  // Hence they will not be written out.
  user_prefs_->ScheduleWritePrefs();
}

PrefStore::PrefReadError PrefValueStore::ReadPrefs() {
  PrefStore::PrefReadError managed_pref_error = PrefStore::PREF_READ_ERROR_NONE;
  PrefStore::PrefReadError user_pref_error = PrefStore::PREF_READ_ERROR_NONE;
  PrefStore::PrefReadError recommended_pref_error =
      PrefStore::PREF_READ_ERROR_NONE;

  // Read managed preferences.
  if (managed_prefs_.get()) {
    managed_pref_error = managed_prefs_->ReadPrefs();
  }

  // Read preferences set by the user.
  if (user_prefs_.get()) {
    user_pref_error = user_prefs_->ReadPrefs();
  }

  // Read recommended preferences.
  if (recommended_prefs_.get()) {
    recommended_pref_error = recommended_prefs_->ReadPrefs();
  }

  // TODO(markusheintz): Return a better error status maybe a struct with
  //   the error status of all PrefStores.

  // Return the first pref store error that occured.
  if (managed_pref_error != PrefStore::PREF_READ_ERROR_NONE) {
    return managed_pref_error;
  }
  if (user_pref_error != PrefStore::PREF_READ_ERROR_NONE) {
    return user_pref_error;
  }
  return recommended_pref_error;
}

bool PrefValueStore::HasPrefPath(const wchar_t* path) const {
  Value* tmp_value = NULL;
  const std::wstring name(path);
  bool rv = GetValue(name, &tmp_value);
  return rv;
}

// The value of a Preference is managed if the PrefStore for managed
// preferences contains a value for the given preference |name|.
bool PrefValueStore::PrefValueIsManaged(const wchar_t* name) {
  if (managed_prefs_.get() == NULL) {
    // No managed PreferenceStore set, hence there are no managed
    // preferences.
    return false;
  }
  Value* tmp_value;
  return managed_prefs_->prefs()->Get(name, &tmp_value);
}

// Note the |DictionaryValue| referenced by the |PrefStore| user_prefs_
// (returned by the method Prefs()) takes the ownership of the Value referenced
// by in_value.
void PrefValueStore::SetUserPrefValue(const wchar_t* name, Value* in_value) {
  user_prefs_->prefs()->Set(name, in_value);
}

bool PrefValueStore::ReadOnly() {
  return user_prefs_->ReadOnly();
}

void PrefValueStore::RemoveUserPrefValue(const wchar_t* name) {
  if (user_prefs_.get()) {
    user_prefs_->prefs()->Remove(name, NULL);
  }
}
