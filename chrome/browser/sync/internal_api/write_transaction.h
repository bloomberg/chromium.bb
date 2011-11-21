// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_WRITE_TRANSACTION_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_WRITE_TRANSACTION_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/internal_api/base_transaction.h"

namespace syncable {
class BaseTransaction;
class WriteTransaction;
}  // namespace syncable

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace sync_api {

// Sync API's WriteTransaction is a read/write BaseTransaction.  It wraps
// a syncable::WriteTransaction.
//
// NOTE: Only a single model type can be mutated for a given
// WriteTransaction.
class WriteTransaction : public BaseTransaction {
 public:
  // Start a new read/write transaction.
  WriteTransaction(const tracked_objects::Location& from_here,
                   UserShare* share);
  virtual ~WriteTransaction();

  // Provide access to the syncable.h transaction from the API WriteNode.
  virtual syncable::BaseTransaction* GetWrappedTrans() const OVERRIDE;
  syncable::WriteTransaction* GetWrappedWriteTrans() { return transaction_; }

 protected:
  WriteTransaction() {}

  void SetTransaction(syncable::WriteTransaction* trans) {
      transaction_ = trans;
  }

 private:
  void* operator new(size_t size);  // Transaction is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::WriteTransaction* transaction_;

  DISALLOW_COPY_AND_ASSIGN(WriteTransaction);
};

} // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_WRITE_TRANSACTION_H_
