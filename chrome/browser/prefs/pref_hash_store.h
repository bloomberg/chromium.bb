// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_HASH_STORE_H_
#define CHROME_BROWSER_PREFS_PREF_HASH_STORE_H_

#include "base/memory/scoped_ptr.h"

class PrefHashStoreTransaction;

// Stores hashes of and verifies preference values via
// PrefHashStoreTransactions.
class PrefHashStore {
 public:
  virtual ~PrefHashStore() {}

  // Returns a PrefHashStoreTransaction which can be used to perform a series
  // of checks/transformations on the hash store.
  virtual scoped_ptr<PrefHashStoreTransaction> BeginTransaction() = 0;

  // Commits this store to disk if it has changed since the last call to this
  // method.
  virtual void CommitPendingWrite() = 0;
};

#endif  // CHROME_BROWSER_PREFS_PREF_HASH_STORE_H_
