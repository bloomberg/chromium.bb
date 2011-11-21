// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_READ_TRANSACTION_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_READ_TRANSACTION_H_

#include "base/compiler_specific.h"
#include "chrome/browser/sync/internal_api/base_transaction.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace sync_api {

struct UserShare;

// Sync API's ReadTransaction is a read-only BaseTransaction.  It wraps
// a syncable::ReadTransaction.
class ReadTransaction : public BaseTransaction {
 public:
  // Start a new read-only transaction on the specified repository.
  ReadTransaction(const tracked_objects::Location& from_here,
                  UserShare* share);

  // Resume the middle of a transaction. Will not close transaction.
  ReadTransaction(UserShare* share, syncable::BaseTransaction* trans);

  virtual ~ReadTransaction();

  // BaseTransaction override.
  virtual syncable::BaseTransaction* GetWrappedTrans() const OVERRIDE;
 private:
  void* operator new(size_t size);  // Transaction is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::BaseTransaction* transaction_;
  bool close_transaction_;

  DISALLOW_COPY_AND_ASSIGN(ReadTransaction);
};

} // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_READ_TRANSACTION_H_
