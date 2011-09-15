// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_CHANGE_DELEGATE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_CHANGE_DELEGATE_H_
#pragma once

#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace syncable {

// This is an interface for listening to directory change events, triggered by
// the releasing of the syncable transaction. The delegate performs work to
// 1. Calculate changes, depending on the source of the transaction
//    (HandleCalculateChangesChangeEventFromSyncer/Syncapi).
// 2. Perform final work while the transaction is held
//    (HandleTransactionEndingChangeEvent).
// 3. Perform any work that should be done after the transaction is released.
//    (HandleTransactionCompleteChangeEvent).
//
// Note that these methods may be called on *any* thread.
class DirectoryChangeDelegate {
 public:
  virtual void HandleCalculateChangesChangeEventFromSyncApi(
      const EntryKernelMutationMap& mutations,
      BaseTransaction* trans) = 0;
  virtual void HandleCalculateChangesChangeEventFromSyncer(
      const EntryKernelMutationMap& mutations,
      BaseTransaction* trans) = 0;
  // Must return the set of all ModelTypes that were modified in the
  // transaction.
  virtual ModelTypeBitSet HandleTransactionEndingChangeEvent(
      BaseTransaction* trans) = 0;
  virtual void HandleTransactionCompleteChangeEvent(
      const ModelTypeBitSet& models_with_changes) = 0;
 protected:
  virtual ~DirectoryChangeDelegate() {}
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_CHANGE_DELEGATE_H_
