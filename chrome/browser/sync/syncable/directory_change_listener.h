// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_CHANGE_LISTENER_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_CHANGE_LISTENER_H_
#pragma once

#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace syncable {

// This is an interface for listening to directory change events, triggered by
// the releasing of the syncable transaction. The listener performs work to
// 1. Calculate changes, depending on the source of the transaction
//    (HandleCalculateChangesChangeEventFromSyncer/Syncapi).
// 2. Perform final work while the transaction is held
//    (HandleTransactionEndingChangeEvent).
// 3. Perform any work that should be done after the transaction is released.
//    (HandleTransactionCompleteChangeEvent).
class DirectoryChangeListener {
 public:
  virtual void HandleCalculateChangesChangeEventFromSyncApi(
      const OriginalEntries& originals,
      const WriterTag& writer,
      BaseTransaction* trans) = 0;
  virtual void HandleCalculateChangesChangeEventFromSyncer(
      const OriginalEntries& originals,
      const WriterTag& writer,
      BaseTransaction* trans) = 0;
  virtual ModelTypeBitSet HandleTransactionEndingChangeEvent(
      BaseTransaction* trans) = 0;
  virtual void HandleTransactionCompleteChangeEvent(
      const ModelTypeBitSet& models_with_changes) = 0;
 protected:
  virtual ~DirectoryChangeListener() {}
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_CHANGE_LISTENER_H_
