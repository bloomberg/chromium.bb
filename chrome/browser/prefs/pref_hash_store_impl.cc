// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_store_impl.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store_transaction.h"
#include "chrome/common/pref_names.h"

namespace internals {

const char kHashOfHashesDict[] = "hash_of_hashes";
const char kStoreVersionsDict[] = "store_versions";

}  // namespace internals

class PrefHashStoreImpl::PrefHashStoreTransactionImpl
    : public PrefHashStoreTransaction {
 public:
  // Constructs a PrefHashStoreTransactionImpl which can use the private
  // members of its |outer| PrefHashStoreImpl.
  explicit PrefHashStoreTransactionImpl(PrefHashStoreImpl* outer);
  virtual ~PrefHashStoreTransactionImpl();

  // PrefHashStoreTransaction implementation.
  virtual ValueState CheckValue(const std::string& path,
                                const base::Value* value) const OVERRIDE;
  virtual void StoreHash(const std::string& path,
                         const base::Value* value) OVERRIDE;
  virtual ValueState CheckSplitValue(
      const std::string& path,
      const base::DictionaryValue* initial_split_value,
      std::vector<std::string>* invalid_keys) const OVERRIDE;
  virtual void StoreSplitHash(
      const std::string& path,
      const base::DictionaryValue* split_value) OVERRIDE;

 private:
  // Clears any hashes stored for |path| through |update|.
  void ClearPath(const std::string& path,
                 DictionaryPrefUpdate* update);

  // Returns true if there are split hashes stored for |path|.
  bool HasSplitHashesAtPath(const std::string& path) const;

  // Used by StoreHash and StoreSplitHash to store the hash of |new_value| at
  // |path| under |update|. Allows multiple hashes to be stored under the same
  // |update|.
  void StoreHashInternal(const std::string& path,
                         const base::Value* new_value,
                         DictionaryPrefUpdate* update);

  PrefHashStoreImpl* outer_;
  bool has_changed_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashStoreTransactionImpl);
};

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

// static
void PrefHashStoreImpl::ResetAllPrefHashStores(PrefService* local_state) {
  local_state->ClearPref(prefs::kProfilePreferenceHashes);
}

void PrefHashStoreImpl::Reset() {
  DictionaryPrefUpdate update(local_state_, prefs::kProfilePreferenceHashes);

  // Remove the dictionary corresponding to the profile name, which may have a
  // '.'
  update->RemoveWithoutPathExpansion(hash_store_id_, NULL);

  // Remove this store's entry in the kStoreVersionsDict.
  base::DictionaryValue* version_dict;
  if (update->GetDictionary(internals::kStoreVersionsDict, &version_dict))
    version_dict->RemoveWithoutPathExpansion(hash_store_id_, NULL);

  // Remove this store's entry in the kHashOfHashesDict.
  base::DictionaryValue* hash_of_hashes_dict;
  if (update->GetDictionaryWithoutPathExpansion(internals::kHashOfHashesDict,
                                                &hash_of_hashes_dict)) {
    hash_of_hashes_dict->RemoveWithoutPathExpansion(hash_store_id_, NULL);
  }
}

scoped_ptr<PrefHashStoreTransaction> PrefHashStoreImpl::BeginTransaction() {
  return scoped_ptr<PrefHashStoreTransaction>(
      new PrefHashStoreTransactionImpl(this));
}

PrefHashStoreImpl::StoreVersion PrefHashStoreImpl::GetCurrentVersion() const {
  const base::DictionaryValue* pref_hash_data =
      local_state_->GetDictionary(prefs::kProfilePreferenceHashes);

  if (!pref_hash_data->GetDictionaryWithoutPathExpansion(hash_store_id_, NULL))
    return VERSION_UNINITIALIZED;

  const base::DictionaryValue* version_dict;
  int current_version;
  if (!pref_hash_data->GetDictionary(internals::kStoreVersionsDict,
                                     &version_dict) ||
      !version_dict->GetIntegerWithoutPathExpansion(hash_store_id_,
                                                    &current_version)) {
    return VERSION_PRE_MIGRATION;
  }

  DCHECK_GT(current_version, VERSION_PRE_MIGRATION);
  return static_cast<StoreVersion>(current_version);
}

bool PrefHashStoreImpl::IsHashDictionaryTrusted() const {
  const base::DictionaryValue* pref_hash_data =
      local_state_->GetDictionary(prefs::kProfilePreferenceHashes);
  const base::DictionaryValue* hashes_dict = NULL;
  const base::DictionaryValue* hash_of_hashes_dict = NULL;
  std::string hash_of_hashes;
  // The absence of the hashes dictionary isn't trusted. Nor is the absence of
  // the hash of hashes for this |hash_store_id_|.
  if (!pref_hash_data->GetDictionaryWithoutPathExpansion(
          hash_store_id_, &hashes_dict) ||
      !pref_hash_data->GetDictionaryWithoutPathExpansion(
          internals::kHashOfHashesDict, &hash_of_hashes_dict) ||
      !hash_of_hashes_dict->GetStringWithoutPathExpansion(
          hash_store_id_, &hash_of_hashes)) {
    return false;
  }

  return pref_hash_calculator_.Validate(
      hash_store_id_, hashes_dict, hash_of_hashes) == PrefHashCalculator::VALID;
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::PrefHashStoreTransactionImpl(
    PrefHashStoreImpl* outer) : outer_(outer), has_changed_(false) {
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::
    ~PrefHashStoreTransactionImpl() {
  DictionaryPrefUpdate update(outer_->local_state_,
                              prefs::kProfilePreferenceHashes);
  // Update the hash_of_hashes if and only if the hashes dictionary has been
  // modified in this transaction.
  if (has_changed_) {
    // Get the dictionary where the hash of hashes are stored.
    base::DictionaryValue* hash_of_hashes_dict = NULL;
    if (!update->GetDictionaryWithoutPathExpansion(internals::kHashOfHashesDict,
                                                   &hash_of_hashes_dict)) {
      hash_of_hashes_dict = new base::DictionaryValue;
      update->SetWithoutPathExpansion(internals::kHashOfHashesDict,
                                      hash_of_hashes_dict);
    }

    // Get the dictionary of hashes (or NULL if it doesn't exist).
    base::DictionaryValue* hashes_dict = NULL;
    update->GetDictionaryWithoutPathExpansion(outer_->hash_store_id_,
                                              &hashes_dict);

    // Use the |hash_store_id_| as the hashed path to avoid having the hash
    // depend on kProfilePreferenceHashes.
    std::string hash_of_hashes(outer_->pref_hash_calculator_.Calculate(
        outer_->hash_store_id_, hashes_dict));
    hash_of_hashes_dict->SetStringWithoutPathExpansion(outer_->hash_store_id_,
                                                       hash_of_hashes);
  }

  // Mark this hash store has having been updated to the latest version (in
  // practice only initialization transactions will actually do this, but
  // since they always occur before minor update transaction it's okay
  // to unconditionally do this here). Do this even if |!has_changed_| to also
  // seed version number on unchanged profiles.
  base::DictionaryValue* store_versions_dict = NULL;
  if (!update->GetDictionary(internals::kStoreVersionsDict,
                             &store_versions_dict)) {
    store_versions_dict = new base::DictionaryValue;
    update->Set(internals::kStoreVersionsDict, store_versions_dict);
  }
  store_versions_dict->SetIntegerWithoutPathExpansion(outer_->hash_store_id_,
                                                      VERSION_LATEST);
}

PrefHashStoreTransaction::ValueState
PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckValue(
    const std::string& path, const base::Value* initial_value) const {
  const base::DictionaryValue* pref_hash_data =
      outer_->local_state_->GetDictionary(prefs::kProfilePreferenceHashes);
  const base::DictionaryValue* hashed_prefs = NULL;
  pref_hash_data->GetDictionaryWithoutPathExpansion(outer_->hash_store_id_,
                                                     &hashed_prefs);

  std::string last_hash;
  if (!hashed_prefs || !hashed_prefs->GetString(path, &last_hash)) {
    // In the absence of a hash for this pref, always trust a NULL value, but
    // only trust an existing value if the initial hashes dictionary is trusted.
    return (!initial_value || outer_->initial_hashes_dictionary_trusted_) ?
               TRUSTED_UNKNOWN_VALUE : UNTRUSTED_UNKNOWN_VALUE;
  }

  PrefHashCalculator::ValidationResult validation_result =
      outer_->pref_hash_calculator_.Validate(path, initial_value, last_hash);
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

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreHash(
    const std::string& path, const base::Value* new_value) {
  DictionaryPrefUpdate update(outer_->local_state_,
                              prefs::kProfilePreferenceHashes);
  StoreHashInternal(path, new_value, &update);
}

PrefHashStoreTransaction::ValueState
PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckSplitValue(
    const std::string& path,
    const base::DictionaryValue* initial_split_value,
    std::vector<std::string>* invalid_keys) const {
  DCHECK(invalid_keys && invalid_keys->empty());

  const bool has_hashes = HasSplitHashesAtPath(path);

  // Treat NULL and empty the same; otherwise we would need to store a hash
  // for the entire dictionary (or some other special beacon) to
  // differentiate these two cases which are really the same for
  // dictionaries.
  if (!initial_split_value || initial_split_value->empty())
    return has_hashes ? CLEARED : UNCHANGED;

  if (!has_hashes) {
    return outer_->initial_hashes_dictionary_trusted_ ?
        TRUSTED_UNKNOWN_VALUE : UNTRUSTED_UNKNOWN_VALUE;
  }

  std::string keyed_path(path);
  keyed_path.push_back('.');
  const size_t common_part_length = keyed_path.length();
  for (base::DictionaryValue::Iterator it(*initial_split_value); !it.IsAtEnd();
       it.Advance()) {
    // Keep the common part from the old |keyed_path| and replace the key to
    // get the new |keyed_path|.
    keyed_path.replace(common_part_length, std::string::npos, it.key());
    ValueState value_state = CheckValue(keyed_path, &it.value());
    switch (value_state) {
      case CLEARED:
        // CLEARED doesn't make sense as a NULL value would never be sampled
        // by the DictionaryValue::Iterator; in fact it is a known weakness of
        // this current algorithm to not detect the case where a single key is
        // cleared entirely from the dictionary pref.
        NOTREACHED();
        break;
      case MIGRATED:
        // Split tracked preferences were introduced after the migration started
        // so no migration is expected, but declare it invalid in Release builds
        // anyways.
        NOTREACHED();
        invalid_keys->push_back(it.key());
        break;
      case UNCHANGED:
        break;
      case CHANGED:  // Falls through.
      case UNTRUSTED_UNKNOWN_VALUE:  // Falls through.
      case TRUSTED_UNKNOWN_VALUE:
        // Declare this value invalid, whether it was changed or never seen
        // before (note that the initial hashes being trusted is irrelevant for
        // individual entries in this scenario).
        invalid_keys->push_back(it.key());
        break;
    }
  }
  return invalid_keys->empty() ? UNCHANGED : CHANGED;
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreSplitHash(
    const std::string& path,
    const base::DictionaryValue* split_value) {
  DictionaryPrefUpdate update(outer_->local_state_,
                              prefs::kProfilePreferenceHashes);
  ClearPath(path, &update);

  if (split_value) {
    std::string keyed_path(path);
    keyed_path.push_back('.');
    const size_t common_part_length = keyed_path.length();
    for (base::DictionaryValue::Iterator it(*split_value); !it.IsAtEnd();
         it.Advance()) {
      // Keep the common part from the old |keyed_path| and replace the key to
      // get the new |keyed_path|.
      keyed_path.replace(common_part_length, std::string::npos, it.key());
      StoreHashInternal(keyed_path, &it.value(), &update);
    }
  }
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ClearPath(
    const std::string& path,
    DictionaryPrefUpdate* update) {
  base::DictionaryValue* hashes_dict = NULL;
  if (update->Get()->GetDictionaryWithoutPathExpansion(outer_->hash_store_id_,
                                                       &hashes_dict)) {
    hashes_dict->Remove(path, NULL);
    has_changed_ = true;
  }
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::HasSplitHashesAtPath(
    const std::string& path) const {
  const base::DictionaryValue* pref_hash_data =
      outer_->local_state_->GetDictionary(prefs::kProfilePreferenceHashes);
  const base::DictionaryValue* hashed_prefs = NULL;
  pref_hash_data->GetDictionaryWithoutPathExpansion(outer_->hash_store_id_,
                                                     &hashed_prefs);
  return hashed_prefs && hashed_prefs->GetDictionary(path, NULL);
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreHashInternal(
    const std::string& path,
    const base::Value* new_value,
    DictionaryPrefUpdate* update) {
  base::DictionaryValue* hashes_dict = NULL;

  // Get the dictionary corresponding to the profile name, which may have a '.'
  if (!update->Get()->GetDictionaryWithoutPathExpansion(outer_->hash_store_id_,
                                                        &hashes_dict)) {
    hashes_dict = new base::DictionaryValue;
    update->Get()->SetWithoutPathExpansion(outer_->hash_store_id_, hashes_dict);
  }
  hashes_dict->SetString(
      path, outer_->pref_hash_calculator_.Calculate(path, new_value));

  has_changed_ = true;
}
