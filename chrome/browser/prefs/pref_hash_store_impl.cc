// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_store_impl.h"

#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"

PrefHashStoreImpl::PrefHashStoreImpl(const std::string& hash_store_id,
                                     const std::string& seed,
                                     const std::string& device_id,
                                     PrefService* local_state)
    : hash_store_id_(hash_store_id),
      pref_hash_calculator_(seed, device_id),
      local_state_(local_state) {}

// static
void PrefHashStoreImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  // Register the top level dictionary to map profile names to dictionaries of
  // tracked preferences.
  registry->RegisterDictionaryPref(prefs::kProfilePreferenceHashes);
}

PrefHashStore::ValueState PrefHashStoreImpl::CheckValue(
    const std::string& path, const base::Value* initial_value) const {
  const base::DictionaryValue* pref_hash_dicts =
      local_state_->GetDictionary(prefs::kProfilePreferenceHashes);
  const base::DictionaryValue* hashed_prefs = NULL;
  pref_hash_dicts->GetDictionaryWithoutPathExpansion(hash_store_id_,
                                                     &hashed_prefs);

  std::string last_hash;
  if (!hashed_prefs || !hashed_prefs->GetString(path, &last_hash))
    return PrefHashStore::UNKNOWN_VALUE;

  PrefHashCalculator::ValidationResult validation_result =
      pref_hash_calculator_.Validate(path, initial_value, last_hash);
  switch (validation_result) {
    case PrefHashCalculator::VALID:
      return PrefHashStore::UNCHANGED;
    case PrefHashCalculator::VALID_LEGACY:
      return PrefHashStore::MIGRATED;
    case PrefHashCalculator::INVALID:
      return initial_value ? PrefHashStore::CHANGED : PrefHashStore::CLEARED;
  }
  NOTREACHED() << "Unexpected PrefHashCalculator::ValidationResult: "
               << validation_result;
  return PrefHashStore::UNKNOWN_VALUE;
}

void PrefHashStoreImpl::StoreHash(
    const std::string& path, const base::Value* new_value) {
  {
    DictionaryPrefUpdate update(local_state_, prefs::kProfilePreferenceHashes);
    DictionaryValue* child_dictionary = NULL;

    // Get the dictionary corresponding to the profile name, which may have a
    // '.'
    if (!update->GetDictionaryWithoutPathExpansion(hash_store_id_,
                                                   &child_dictionary)) {
      child_dictionary = new DictionaryValue;
      update->SetWithoutPathExpansion(hash_store_id_, child_dictionary);
    }

    child_dictionary->SetString(
        path, pref_hash_calculator_.Calculate(path, new_value));
  }
  // TODO(erikwright): During tests, pending writes were still waiting when the
  // IO thread is already gone. Consider other solutions.
  local_state_->CommitPendingWrite();
}
