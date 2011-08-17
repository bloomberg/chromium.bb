// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_BASE_TRANSACTION_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_BASE_TRANSACTION_H_
#pragma once

#include "chrome/browser/sync/internal_api/user_share.h"

#include "chrome/browser/sync/util/cryptographer.h"

namespace syncable {
class BaseTransaction;
class ScopedDirLookup;
}

namespace sync_api {

// Sync API's BaseTransaction, ReadTransaction, and WriteTransaction allow for
// batching of several read and/or write operations.  The read and write
// operations are performed by creating ReadNode and WriteNode instances using
// the transaction. These transaction classes wrap identically named classes in
// syncable, and are used in a similar way. Unlike syncable::BaseTransaction,
// whose construction requires an explicit syncable::ScopedDirLookup, a sync
// API BaseTransaction creates its own ScopedDirLookup implicitly.
class BaseTransaction {
 public:
  // Provide access to the underlying syncable.h objects from BaseNode.
  virtual syncable::BaseTransaction* GetWrappedTrans() const = 0;
  const syncable::ScopedDirLookup& GetLookup() const { return *lookup_; }
  browser_sync::Cryptographer* GetCryptographer() const {
    return cryptographer_;
  }

 protected:
  // The ScopedDirLookup is created in the constructor and destroyed
  // in the destructor.  Creation of the ScopedDirLookup is not expected
  // to fail.
  explicit BaseTransaction(UserShare* share);
  virtual ~BaseTransaction();

  BaseTransaction() { lookup_= NULL; }

 private:
  // A syncable ScopedDirLookup, which is the parent of syncable transactions.
  syncable::ScopedDirLookup* lookup_;

  browser_sync::Cryptographer* cryptographer_;

  DISALLOW_COPY_AND_ASSIGN(BaseTransaction);
};

syncable::ModelTypeSet GetEncryptedTypes(
    const sync_api::BaseTransaction* trans);

}  // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_BASE_TRANSACTION_H_
