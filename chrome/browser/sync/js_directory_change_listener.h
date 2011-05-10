// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_DIRECTORY_CHANGE_LISTENER_H_
#define CHROME_BROWSER_SYNC_JS_DIRECTORY_CHANGE_LISTENER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/syncable/directory_change_listener.h"

namespace browser_sync {

class JsEventRouter;

// Routes SyncManager events to a JsEventRouter.
class JsDirectoryChangeListener : public syncable::DirectoryChangeListener {
 public:
  // |parent_router| must be non-NULL and must outlive this object.
  explicit JsDirectoryChangeListener(JsEventRouter* parent_router);

  virtual ~JsDirectoryChangeListener();

  // syncable::DirectoryChangeListener implementation.
  virtual void HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::OriginalEntries& originals,
      const syncable::WriterTag& writer,
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual void HandleCalculateChangesChangeEventFromSyncer(
      const syncable::OriginalEntries& originals,
      const syncable::WriterTag& writer,
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual syncable::ModelTypeBitSet HandleTransactionEndingChangeEvent(
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual void HandleTransactionCompleteChangeEvent(
      const syncable::ModelTypeBitSet& models_with_changes) OVERRIDE;

 private:
  JsEventRouter* parent_router_;

  DISALLOW_COPY_AND_ASSIGN(JsDirectoryChangeListener);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_DIRECTORY_CHANGE_LISTENER_H_
