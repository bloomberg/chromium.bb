// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_store_impl.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store_transaction.h"
#include "chrome/browser/prefs/tracked/hash_store_contents.h"

namespace {

// Returns true if the dictionary of hashes stored in |contents| is trusted
// (which implies unknown values can be trusted as newly tracked values).
bool IsHashDictionaryTrusted(const PrefHashCalculator& calculator,
                             const HashStoreContents& contents) {
  const base::DictionaryValue* store_contents = contents.GetContents();
  std::string super_mac = contents.GetSuperMac();
  // The store must be initialized and have a valid super MAC to be trusted.
  return store_contents && !super_mac.empty() &&
         calculator.Validate(contents.hash_store_id(),
                             store_contents,
                             super_mac) == PrefHashCalculator::VALID;
}

}  // namespace

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
  bool GetSplitMacs(const std::string& path,
                    std::map<std::string, std::string>* split_macs) const;
  PrefHashStoreImpl* outer_;
  bool has_changed_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashStoreTransactionImpl);
};

PrefHashStoreImpl::PrefHashStoreImpl(const std::string& seed,
                                     const std::string& device_id,
                                     scoped_ptr<HashStoreContents> contents)
    : pref_hash_calculator_(seed, device_id),
      contents_(contents.Pass()),
      initial_hashes_dictionary_trusted_(
          IsHashDictionaryTrusted(pref_hash_calculator_, *contents_)),
      has_pending_write_(false) {
  DCHECK(contents_);
  UMA_HISTOGRAM_BOOLEAN("Settings.HashesDictionaryTrusted",
                        initial_hashes_dictionary_trusted_);
}

PrefHashStoreImpl::~PrefHashStoreImpl() {}

void PrefHashStoreImpl::Reset() {
  contents_->Reset();
}

scoped_ptr<PrefHashStoreTransaction> PrefHashStoreImpl::BeginTransaction() {
  return scoped_ptr<PrefHashStoreTransaction>(
      new PrefHashStoreTransactionImpl(this));
}

PrefHashStoreImpl::StoreVersion PrefHashStoreImpl::GetCurrentVersion() const {
  if (!contents_->IsInitialized())
    return VERSION_UNINITIALIZED;

  int current_version;
  if (!contents_->GetVersion(&current_version)) {
    return VERSION_PRE_MIGRATION;
  }

  DCHECK_GT(current_version, VERSION_PRE_MIGRATION);
  return static_cast<StoreVersion>(current_version);
}

void PrefHashStoreImpl::CommitPendingWrite() {
  if (has_pending_write_) {
    contents_->CommitPendingWrite();
    has_pending_write_ = false;
  }
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::PrefHashStoreTransactionImpl(
    PrefHashStoreImpl* outer) : outer_(outer), has_changed_(false) {
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::
    ~PrefHashStoreTransactionImpl() {
  // Update the super MAC if and only if the hashes dictionary has been
  // modified in this transaction.
  if (has_changed_) {
    // Get the dictionary of hashes (or NULL if it doesn't exist).
    const base::DictionaryValue* hashes_dict = outer_->contents_->GetContents();
    outer_->contents_->SetSuperMac(outer_->pref_hash_calculator_.Calculate(
        outer_->contents_->hash_store_id(), hashes_dict));

    outer_->has_pending_write_ = true;
  }

  // Mark this hash store has having been updated to the latest version (in
  // practice only initialization transactions will actually do this, but
  // since they always occur before minor update transaction it's okay
  // to unconditionally do this here). Only do this if this store's version
  // isn't already at VERSION_LATEST (to avoid scheduling a write when
  // unecessary). Note, this is outside of |if (has_changed)| to also seed
  // version number of otherwise unchanged profiles.
  int current_version;
  if (!outer_->contents_->GetVersion(&current_version) ||
      current_version != VERSION_LATEST) {
    outer_->contents_->SetVersion(VERSION_LATEST);
    outer_->has_pending_write_ = true;
  }
}

PrefHashStoreTransaction::ValueState
PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckValue(
    const std::string& path, const base::Value* initial_value) const {
  const base::DictionaryValue* hashed_prefs = outer_->contents_->GetContents();

  std::string last_hash;
  if (hashed_prefs)
    hashed_prefs->GetString(path, &last_hash);

  if (last_hash.empty()) {
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
    case PrefHashCalculator::VALID_WEAK_LEGACY:
      return WEAK_LEGACY;
    case PrefHashCalculator::VALID_SECURE_LEGACY:
      return SECURE_LEGACY;
    case PrefHashCalculator::INVALID:
      return initial_value ? CHANGED : CLEARED;
  }
  NOTREACHED() << "Unexpected PrefHashCalculator::ValidationResult: "
               << validation_result;
  return UNTRUSTED_UNKNOWN_VALUE;
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreHash(
    const std::string& path, const base::Value* new_value) {
  const std::string mac =
      outer_->pref_hash_calculator_.Calculate(path, new_value);
  (*outer_->contents_->GetMutableContents())->SetString(path, mac);
  has_changed_ = true;
}

PrefHashStoreTransaction::ValueState
PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckSplitValue(
    const std::string& path,
    const base::DictionaryValue* initial_split_value,
    std::vector<std::string>* invalid_keys) const {
  DCHECK(invalid_keys && invalid_keys->empty());

  std::map<std::string, std::string> split_macs;
  const bool has_hashes = GetSplitMacs(path, &split_macs);

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

  bool has_secure_legacy_id_hashes = false;
  std::string keyed_path(path);
  keyed_path.push_back('.');
  const size_t common_part_length = keyed_path.length();
  for (base::DictionaryValue::Iterator it(*initial_split_value); !it.IsAtEnd();
       it.Advance()) {
    std::map<std::string, std::string>::iterator entry =
        split_macs.find(it.key());
    if (entry == split_macs.end()) {
      invalid_keys->push_back(it.key());
    } else {
      // Keep the common part from the old |keyed_path| and replace the key to
      // get the new |keyed_path|.
      keyed_path.replace(common_part_length, std::string::npos, it.key());
      switch (outer_->pref_hash_calculator_.Validate(
          keyed_path, &it.value(), entry->second)) {
        case PrefHashCalculator::VALID:
          break;
        case WEAK_LEGACY:
          // Split tracked preferences were introduced after the migration from
          // the weaker legacy algorithm started so no migration is expected,
          // but declare it invalid in Release builds anyways.
          NOTREACHED();
          invalid_keys->push_back(it.key());
          break;
        case SECURE_LEGACY:
          // Secure legacy device IDs based hashes are still accepted, but we
          // should make sure to notify the caller for him to update the legacy
          // hashes.
          has_secure_legacy_id_hashes = true;
          break;
        case PrefHashCalculator::INVALID:
          invalid_keys->push_back(it.key());
          break;
      }
      // Remove processed MACs, remaining MACs at the end will also be
      // considered invalid.
      split_macs.erase(entry);
    }
  }

  // Anything left in the map is missing from the data.
  for (std::map<std::string, std::string>::const_iterator it =
           split_macs.begin();
       it != split_macs.end();
       ++it) {
    invalid_keys->push_back(it->first);
  }

  return invalid_keys->empty()
      ? (has_secure_legacy_id_hashes ? SECURE_LEGACY : UNCHANGED)
      : CHANGED;
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreSplitHash(
    const std::string& path,
    const base::DictionaryValue* split_value) {
  scoped_ptr<HashStoreContents::MutableDictionary> mutable_dictionary =
      outer_->contents_->GetMutableContents();
  (*mutable_dictionary)->Remove(path, NULL);

  if (split_value) {
    std::string keyed_path(path);
    keyed_path.push_back('.');
    const size_t common_part_length = keyed_path.length();
    for (base::DictionaryValue::Iterator it(*split_value); !it.IsAtEnd();
         it.Advance()) {
      // Keep the common part from the old |keyed_path| and replace the key to
      // get the new |keyed_path|.
      keyed_path.replace(common_part_length, std::string::npos, it.key());
      (*mutable_dictionary)->SetString(
          keyed_path,
          outer_->pref_hash_calculator_.Calculate(keyed_path, &it.value()));
    }
  }
  has_changed_ = true;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::GetSplitMacs(
    const std::string& key,
    std::map<std::string, std::string>* split_macs) const {
  DCHECK(split_macs);
  DCHECK(split_macs->empty());

  const base::DictionaryValue* hashed_prefs = outer_->contents_->GetContents();
  const base::DictionaryValue* split_mac_dictionary = NULL;
  if (!hashed_prefs || !hashed_prefs->GetDictionary(key, &split_mac_dictionary))
    return false;
  for (base::DictionaryValue::Iterator it(*split_mac_dictionary); !it.IsAtEnd();
       it.Advance()) {
    std::string mac_string;
    if (!it.value().GetAsString(&mac_string)) {
      NOTREACHED();
      continue;
    }
    split_macs->insert(make_pair(it.key(), mac_string));
  }
  return true;
}
