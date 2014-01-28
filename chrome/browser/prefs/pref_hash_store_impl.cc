// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_store_impl.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
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
      local_state_(local_state),
      initial_hashes_dictionary_trusted_(IsHashDictionaryTrusted()) {
  UMA_HISTOGRAM_BOOLEAN("Settings.HashesDictionaryTrusted",
                        initial_hashes_dictionary_trusted_);
}

// static
void PrefHashStoreImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  // Register the top level dictionary to map profile names to dictionaries of
  // tracked preferences.
  registry->RegisterDictionaryPref(prefs::kProfilePreferenceHashes);
}

void PrefHashStoreImpl::Reset() {
  DictionaryPrefUpdate update(local_state_, prefs::kProfilePreferenceHashes);

  // Remove the dictionary corresponding to the profile name, which may have a
  // '.'
  update->RemoveWithoutPathExpansion(hash_store_id_, NULL);
}

bool PrefHashStoreImpl::IsInitialized() const {
  const base::DictionaryValue* pref_hash_dicts =
      local_state_->GetDictionary(prefs::kProfilePreferenceHashes);
  return pref_hash_dicts &&
      pref_hash_dicts->GetDictionaryWithoutPathExpansion(hash_store_id_, NULL);
}

PrefHashStore::ValueState PrefHashStoreImpl::CheckValue(
    const std::string& path, const base::Value* initial_value) const {
  const base::DictionaryValue* pref_hash_dicts =
      local_state_->GetDictionary(prefs::kProfilePreferenceHashes);
  const base::DictionaryValue* hashed_prefs = NULL;
  pref_hash_dicts->GetDictionaryWithoutPathExpansion(hash_store_id_,
                                                     &hashed_prefs);

  std::string last_hash;
  if (!hashed_prefs || !hashed_prefs->GetString(path, &last_hash)) {
    // In the absence of a hash for this pref, always trust a NULL value, but
    // only trust an existing value if the initial hashes dictionary is trusted.
    return (!initial_value || initial_hashes_dictionary_trusted_) ?
               TRUSTED_UNKNOWN_VALUE : UNTRUSTED_UNKNOWN_VALUE;
  }

  PrefHashCalculator::ValidationResult validation_result =
      pref_hash_calculator_.Validate(path, initial_value, last_hash);
  switch (validation_result) {
    case PrefHashCalculator::VALID:
      return UNCHANGED;
    case PrefHashCalculator::VALID_LEGACY:
      return MIGRATED;
    case PrefHashCalculator::INVALID:
      return initial_value ? CHANGED : CLEARED;
  }
  NOTREACHED() << "Unexpected PrefHashCalculator::ValidationResult: "
               << validation_result;
  return UNTRUSTED_UNKNOWN_VALUE;
}

void PrefHashStoreImpl::StoreHash(
    const std::string& path, const base::Value* new_value) {
  DictionaryPrefUpdate update(local_state_, prefs::kProfilePreferenceHashes);

  // Get the dictionary corresponding to the profile name, which may have a
  // '.'
  base::DictionaryValue* hashes_dict = NULL;
  if (!update->GetDictionaryWithoutPathExpansion(hash_store_id_,
                                                 &hashes_dict)) {
    hashes_dict = new base::DictionaryValue;
    update->SetWithoutPathExpansion(hash_store_id_, hashes_dict);
  }

  hashes_dict->SetString(
      path, pref_hash_calculator_.Calculate(path, new_value));

  // Get the dictionary where the hash of hashes are stored.
  base::DictionaryValue* hash_of_hashes_dict = NULL;
  if (!update->GetDictionaryWithoutPathExpansion(internals::kHashOfHashesPref,
                                                 &hash_of_hashes_dict)) {
    hash_of_hashes_dict = new base::DictionaryValue;
    update->SetWithoutPathExpansion(internals::kHashOfHashesPref,
                                    hash_of_hashes_dict);
  }
  // Use the |hash_store_id_| as the hashed path to avoid having the hash
  // depend on kProfilePreferenceHashes.
  std::string hash_of_hashes(pref_hash_calculator_.Calculate(hash_store_id_,
                                                             hashes_dict));
  hash_of_hashes_dict->SetStringWithoutPathExpansion(hash_store_id_,
                                                     hash_of_hashes);
}

bool PrefHashStoreImpl::IsHashDictionaryTrusted() const {
  const base::DictionaryValue* pref_hash_dicts =
      local_state_->GetDictionary(prefs::kProfilePreferenceHashes);
  const base::DictionaryValue* hashes_dict = NULL;
  const base::DictionaryValue* hash_of_hashes_dict = NULL;
  std::string hash_of_hashes;
  // The absence of the hashes dictionary isn't trusted. Nor is the absence of
  // the hash of hashes for this |hash_store_id_|.
  if (!pref_hash_dicts->GetDictionaryWithoutPathExpansion(
          hash_store_id_, &hashes_dict) ||
      !pref_hash_dicts->GetDictionaryWithoutPathExpansion(
          internals::kHashOfHashesPref, &hash_of_hashes_dict) ||
      !hash_of_hashes_dict->GetStringWithoutPathExpansion(
          hash_store_id_, &hash_of_hashes)) {
    return false;
  }

  return pref_hash_calculator_.Validate(
      hash_store_id_, hashes_dict, hash_of_hashes) == PrefHashCalculator::VALID;
}
