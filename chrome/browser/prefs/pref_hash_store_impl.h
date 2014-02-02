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

class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
class Value;
}

// Implements PrefHashStoreImpl by storing preference hashes in a PrefService.
class PrefHashStoreImpl : public PrefHashStore {
 public:
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
  virtual bool IsInitialized() const OVERRIDE;
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

  // Updates kHashOfHashesPref to reflect the last changes to the |hashes_dict|.
  // Must be called after every change to the |hashes_dict|, within the scope of
  // |update|.
  void UpdateHashOfHashes(const base::DictionaryValue* hashes_dict,
                          DictionaryPrefUpdate* update);

  // Returns true if the dictionary of hashes stored for |hash_store_id_| is
  // trusted (which implies unknown values can be trusted as newly tracked
  // values).
  bool IsHashDictionaryTrusted() const;

  std::string hash_store_id_;
  PrefHashCalculator pref_hash_calculator_;
  PrefService* local_state_;
  bool initial_hashes_dictionary_trusted_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashStoreImpl);
};

#endif  // CHROME_BROWSER_PREFS_PREF_HASH_STORE_IMPL_H_
