// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_TRACKED_HASH_STORE_CONTENTS_H_
#define CHROME_BROWSER_PREFS_TRACKED_HASH_STORE_CONTENTS_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
} // namespace base

// Provides access to the contents of a preference hash store. The store
// contains the following data:
// Contents: a client-defined dictionary that should map preference names to
// MACs.
// Version: a client-defined version number for the format of Contents.
// Super MAC: a MAC that authenticates the entirety of Contents.
// Legacy hash stores may have an ID that was incorporated into MAC
// calculations. The ID, if any, is available via |hash_store_id()|.
class HashStoreContents {
 public:
  // Used to modify a DictionaryValue stored in the preference hash store. The
  // MutableDictionary should be destroyed when modifications are complete in
  // order to allow them to be committed to the underlying storage.
  class MutableDictionary {
   public:
    virtual ~MutableDictionary() {}
    // Returns the DictionaryValue, which will be created if it does not already
    // exist.
    virtual base::DictionaryValue* operator->() = 0;
  };

  virtual ~HashStoreContents() {}

  // Returns the hash-store ID. May be empty.
  virtual std::string hash_store_id() const = 0;

  // Discards all data related to this hash store.
  virtual void Reset() = 0;

  // Indicates whether any data is currently stored for this hash store.
  virtual bool IsInitialized() const = 0;

  // Retrieves a version number previously associated with this hash store via
  // SetVersion. Returns false if no version number has been stored.
  virtual bool GetVersion(int* version) const = 0;

  // Associates a version number wih this hash store.
  virtual void SetVersion(int version) = 0;

  // Retrieves the contents of this hash store. May return NULL if the hash
  // store has not been initialized.
  virtual const base::DictionaryValue* GetContents() const = 0;

  // Provides mutable access to the contents of this hash store.
  virtual scoped_ptr<MutableDictionary> GetMutableContents() = 0;

  // Retrieves the super MAC value previously stored by SetSuperMac. May be
  // empty if no super MAC has been stored.
  virtual std::string GetSuperMac() const = 0;

  // Stores a super MAC value for this hash store.
  virtual void SetSuperMac(const std::string& super_mac) = 0;

  // Commit pending writes to this hash store's contents.
  virtual void CommitPendingWrite() = 0;
};

#endif  // CHROME_BROWSER_PREFS_TRACKED_HASH_STORE_CONTENTS_H_
