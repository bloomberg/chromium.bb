// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_HASH_STORE_IMPL_H_
#define CHROME_BROWSER_PREFS_PREF_HASH_STORE_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prefs/pref_hash_calculator.h"
#include "chrome/browser/prefs/pref_hash_store.h"

class HashStoreContents;
class PrefHashStoreTransaction;

namespace base {
class DictionaryValue;
class Value;
}

// Implements PrefHashStoreImpl by storing preference hashes in a
// HashStoreContents.
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
  // |seed| and |device_id| and stores them in |contents|.
  //
  // The same |seed| and |device_id| must be used to load and validate
  // previously stored hashes in |contents|.
  PrefHashStoreImpl(const std::string& seed,
                    const std::string& device_id,
                    scoped_ptr<HashStoreContents> contents);

  virtual ~PrefHashStoreImpl();

  // Clears the contents of this PrefHashStore. |IsInitialized()| will return
  // false after this call.
  void Reset();

  // PrefHashStore implementation.
  virtual scoped_ptr<PrefHashStoreTransaction> BeginTransaction() OVERRIDE;
  virtual void CommitPendingWrite() OVERRIDE;

  // Returns the current version of this hash store.
  StoreVersion GetCurrentVersion() const;

 private:
  class PrefHashStoreTransactionImpl;

  const PrefHashCalculator pref_hash_calculator_;
  scoped_ptr<HashStoreContents> contents_;
  const bool initial_hashes_dictionary_trusted_;

  // True if hashes have been modified since the last call to
  // CommitPendingWriteIfRequired().
  bool has_pending_write_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashStoreImpl);
};

#endif  // CHROME_BROWSER_PREFS_PREF_HASH_STORE_IMPL_H_
