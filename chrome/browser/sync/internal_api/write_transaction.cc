// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/internal_api/write_transaction.h"

#include "chrome/browser/sync/syncable/syncable.h"

namespace sync_api {

//////////////////////////////////////////////////////////////////////////
// WriteTransaction member definitions
WriteTransaction::WriteTransaction(const tracked_objects::Location& from_here,
                                   UserShare* share)
    : BaseTransaction(share),
      transaction_(NULL) {
  transaction_ = new syncable::WriteTransaction(from_here, syncable::SYNCAPI,
                                                GetLookup());
}

WriteTransaction::~WriteTransaction() {
  delete transaction_;
}

syncable::BaseTransaction* WriteTransaction::GetWrappedTrans() const {
  return transaction_;
}

} // namespace sync_api
