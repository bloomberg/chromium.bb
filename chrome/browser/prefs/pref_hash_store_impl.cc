// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_store_impl.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store_transaction.h"
#include "chrome/browser/prefs/tracked/hash_store_contents.h"

class PrefHashStoreImpl::PrefHashStoreTransactionImpl
    : public PrefHashStoreTransaction {
 public:
  // Constructs a PrefHashStoreTransactionImpl which can use the private
  // members of its |outer| PrefHashStoreImpl.
  PrefHashStoreTransactionImpl(PrefHashStoreImpl* outer,
                               scoped_ptr<HashStoreContents> storage);
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
  virtual bool HasHash(const std::string& path) const OVERRIDE;
  virtual void ImportHash(const std::string& path,
                          const base::Value* hash) OVERRIDE;
  virtual void ClearHash(const std::string& path) OVERRIDE;
  virtual bool IsSuperMACValid() const OVERRIDE;
  virtual bool StampSuperMac() OVERRIDE;

 private:
  bool GetSplitMacs(const std::string& path,
                    std::map<std::string, std::string>* split_macs) const;

  HashStoreContents* contents() {
    return outer_->legacy_hash_store_contents_
               ? outer_->legacy_hash_store_contents_.get()
               : contents_.get();
  }

  const HashStoreContents* contents() const {
    return outer_->legacy_hash_store_contents_
               ? outer_->legacy_hash_store_contents_.get()
               : contents_.get();
  }

  PrefHashStoreImpl* outer_;
  scoped_ptr<HashStoreContents> contents_;

  bool super_mac_valid_;
  bool super_mac_dirty_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashStoreTransactionImpl);
};

PrefHashStoreImpl::PrefHashStoreImpl(const std::string& seed,
                                     const std::string& device_id,
                                     bool use_super_mac)
    : pref_hash_calculator_(seed, device_id),
      use_super_mac_(use_super_mac) {
}

PrefHashStoreImpl::~PrefHashStoreImpl() {
}

void PrefHashStoreImpl::set_legacy_hash_store_contents(
    scoped_ptr<HashStoreContents> legacy_hash_store_contents) {
  legacy_hash_store_contents_ = legacy_hash_store_contents.Pass();
}

scoped_ptr<PrefHashStoreTransaction> PrefHashStoreImpl::BeginTransaction(
    scoped_ptr<HashStoreContents> storage) {
  return scoped_ptr<PrefHashStoreTransaction>(
      new PrefHashStoreTransactionImpl(this, storage.Pass()));
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::PrefHashStoreTransactionImpl(
    PrefHashStoreImpl* outer,
    scoped_ptr<HashStoreContents> storage)
    : outer_(outer),
      contents_(storage.Pass()),
      super_mac_valid_(false),
      super_mac_dirty_(false) {
  if (!outer_->use_super_mac_)
    return;

  // The store must be initialized and have a valid super MAC to be trusted.

  const base::DictionaryValue* store_contents = contents()->GetContents();
  if (!store_contents)
    return;

  std::string super_mac = contents()->GetSuperMac();
  if (super_mac.empty())
    return;

  super_mac_valid_ =
      outer_->pref_hash_calculator_.Validate(
          contents()->hash_store_id(), store_contents, super_mac) ==
      PrefHashCalculator::VALID;
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::
    ~PrefHashStoreTransactionImpl() {
  if (super_mac_dirty_ && outer_->use_super_mac_) {
    // Get the dictionary of hashes (or NULL if it doesn't exist).
    const base::DictionaryValue* hashes_dict = contents()->GetContents();
    contents()->SetSuperMac(outer_->pref_hash_calculator_.Calculate(
        contents()->hash_store_id(), hashes_dict));
  }
}

PrefHashStoreTransaction::ValueState
PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckValue(
    const std::string& path,
    const base::Value* initial_value) const {
  const base::DictionaryValue* hashes_dict = contents()->GetContents();

  std::string last_hash;
  if (hashes_dict)
    hashes_dict->GetString(path, &last_hash);

  if (last_hash.empty()) {
    // In the absence of a hash for this pref, always trust a NULL value, but
    // only trust an existing value if the initial hashes dictionary is trusted.
    return (!initial_value || super_mac_valid_) ? TRUSTED_UNKNOWN_VALUE
                                                : UNTRUSTED_UNKNOWN_VALUE;
  }

  PrefHashCalculator::ValidationResult validation_result =
      outer_->pref_hash_calculator_.Validate(path, initial_value, last_hash);
  switch (validation_result) {
    case PrefHashCalculator::VALID:
      return UNCHANGED;
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
    const std::string& path,
    const base::Value* new_value) {
  const std::string mac =
      outer_->pref_hash_calculator_.Calculate(path, new_value);
  (*contents()->GetMutableContents())->SetString(path, mac);
  super_mac_dirty_ = true;
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

  if (!has_hashes)
    return super_mac_valid_ ? TRUSTED_UNKNOWN_VALUE : UNTRUSTED_UNKNOWN_VALUE;

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
      contents()->GetMutableContents();
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
  super_mac_dirty_ = true;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::GetSplitMacs(
    const std::string& key,
    std::map<std::string, std::string>* split_macs) const {
  DCHECK(split_macs);
  DCHECK(split_macs->empty());

  const base::DictionaryValue* hashes_dict = contents()->GetContents();
  const base::DictionaryValue* split_mac_dictionary = NULL;
  if (!hashes_dict || !hashes_dict->GetDictionary(key, &split_mac_dictionary))
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

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::HasHash(
    const std::string& path) const {
  const base::DictionaryValue* hashes_dict = contents()->GetContents();
  return hashes_dict && hashes_dict->Get(path, NULL);
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ImportHash(
    const std::string& path,
    const base::Value* hash) {
  DCHECK(hash);

  (*contents()->GetMutableContents())->Set(path, hash->DeepCopy());

  if (super_mac_valid_)
    super_mac_dirty_ = true;
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ClearHash(
    const std::string& path) {
  if ((*contents()->GetMutableContents())->RemovePath(path, NULL) &&
      super_mac_valid_) {
    super_mac_dirty_ = true;
  }
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::IsSuperMACValid() const {
  return super_mac_valid_;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::StampSuperMac() {
  if (!outer_->use_super_mac_ || super_mac_valid_)
    return false;
  super_mac_dirty_ = true;
  return true;
}
