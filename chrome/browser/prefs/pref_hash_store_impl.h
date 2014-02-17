// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_HASH_STORE_IMPL_H_
#define CHROME_BROWSER_PREFS_PREF_HASH_STORE_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prefs/pref_hash_calculator.h"
#include "chrome/browser/prefs/pref_hash_store.h"

class PrefHashStoreTransaction;
class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
class Value;
}

namespace internals {

// Hash of hashes for each profile, used to validate the existing hashes when
// debating whether an unknown value is to be trusted, will be stored as a
// string under
// |kProfilePreferenceHashes|.|kHashOfHashesDict|.|hash_stored_id_|.
extern const char kHashOfHashesDict[];

// Versions for each PrefHashStore are stored in this dictionary under
// |hash_store_id_|.
extern const char kStoreVersionsDict[];

}  // namespace internals

// Implements PrefHashStoreImpl by storing preference hashes in a PrefService.
class PrefHashStoreImpl : public PrefHashStore {
 public:
  enum StoreVersion {
    // No hashes have been stored in this PrefHashStore yet.
    VERSION_UNINITIALIZED = 0,
    // The hashes in this PrefHashStore were stored before the introduction
    // of a version number and should be re-initialized.
    VERSION_PRE_MIGRATION = 1,
    // The hashes in this PrefHashStore were stored using the latest algorithm.
    VERSION_LATEST = 2,
  };

  // Constructs a PrefHashStoreImpl that calculates hashes using
  // |seed| and |device_id| and stores them in |local_state|. Multiple hash
  // stores can use the same |local_state| with distinct |hash_store_id|s.
  //
  // The same |seed|, |device_id|, and |hash_store_id| must be used to load and
  // validate previously stored hashes in |local_state|.
  //
  // |local_state| must have previously been passed to |RegisterPrefs|.
  PrefHashStoreImpl(const std::string& hash_store_id,
                    const std::string& seed,
                    const std::string& device_id,
                    PrefService* local_state);

  // Registers required local state preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Clears the contents of this PrefHashStore. |IsInitialized()| will return
  // false after this call.
  void Reset();

  // PrefHashStore implementation.
  virtual scoped_ptr<PrefHashStoreTransaction> BeginTransaction() OVERRIDE;

  // Returns the current version of this hash store.
  StoreVersion GetCurrentVersion() const;

 private:
  class PrefHashStoreTransactionImpl;

  // Returns true if the dictionary of hashes stored for |hash_store_id_| is
  // trusted (which implies unknown values can be trusted as newly tracked
  // values).
  bool IsHashDictionaryTrusted() const;

  const std::string hash_store_id_;
  const PrefHashCalculator pref_hash_calculator_;
  PrefService* local_state_;
  // Must come after |local_state_| and |pref_hash_calculator_| in the
  // initialization list as it depends on them to compute its value via
  // IsHashDictionaryTrusted().
  const bool initial_hashes_dictionary_trusted_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashStoreImpl);
};

#endif  // CHROME_BROWSER_PREFS_PREF_HASH_STORE_IMPL_H_
