// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base_transaction.h"

#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/cryptographer.h"

using browser_sync::Cryptographer;

namespace sync_api {

//////////////////////////////////////////////////////////////////////////
// BaseTransaction member definitions
BaseTransaction::BaseTransaction(UserShare* share)
    : lookup_(NULL) {
  DCHECK(share && share->dir_manager.get());
  lookup_ = new syncable::ScopedDirLookup(share->dir_manager.get(),
                                          share->name);
  cryptographer_ = share->dir_manager->GetCryptographer(this);
  if (!(lookup_->good()))
    DCHECK(false) << "ScopedDirLookup failed on valid DirManager.";
}
BaseTransaction::~BaseTransaction() {
  delete lookup_;
}

syncable::ModelTypeSet GetEncryptedTypes(
    const sync_api::BaseTransaction* trans) {
  Cryptographer* cryptographer = trans->GetCryptographer();
  return cryptographer->GetEncryptedTypes();
}

} // namespace sync_api
