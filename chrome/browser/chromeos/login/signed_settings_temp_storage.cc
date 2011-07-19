// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"

static base::LazyInstance<chromeos::SignedSettings::Delegate<bool> >
    g_signed_settings_delegate(base::LINKER_INITIALIZED);

namespace chromeos {

// static
void SignedSettingsTempStorage::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterDictionaryPref(prefs::kSignedSettingsTempStorage);
}

// static
bool SignedSettingsTempStorage::Store(const std::string& name,
                                      const std::string& value,
                                      PrefService* local_state) {
  if (local_state) {
    DictionaryPrefUpdate temp_storage_update(
        local_state, prefs::kSignedSettingsTempStorage);
    temp_storage_update->SetWithoutPathExpansion(
        name, Value::CreateStringValue(value));
    return true;
  }
  return false;
}

// static
bool SignedSettingsTempStorage::Retrieve(const std::string& name,
                                         std::string* value,
                                         PrefService* local_state) {
  if (local_state) {
    const DictionaryValue* temp_storage =
        local_state->GetDictionary(prefs::kSignedSettingsTempStorage);
    if (temp_storage && temp_storage->HasKey(name)) {
      temp_storage->GetStringWithoutPathExpansion(name, value);
      return true;
    }
  }
  return false;
}

// static
void SignedSettingsTempStorage::Finalize(PrefService* local_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (local_state) {
    const DictionaryValue* temp_storage =
        local_state->GetDictionary(prefs::kSignedSettingsTempStorage);
    if (temp_storage) {
      // We've stored some settings in transient storage
      // before owner has been assigned.
      // Now owner is assigned and key is generated and we should persist
      // those settings into signed storage.
      for (DictionaryValue::key_iterator it = temp_storage->begin_keys();
           it != temp_storage->end_keys();
           ++it) {
        std::string value;
        temp_storage->GetStringWithoutPathExpansion(*it, &value);
        SignedSettings::CreateStorePropertyOp(
            *it, value,
            g_signed_settings_delegate.Pointer())->Execute();
      }
      local_state->ClearPref(prefs::kSignedSettingsTempStorage);
    }
  }
}

}  // namespace chromeos
