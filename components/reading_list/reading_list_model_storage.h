// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_READING_LIST_MODEL_STORAGE_H_
#define COMPONENTS_READING_LIST_READING_LIST_MODEL_STORAGE_H_

#include <vector>

#include "components/reading_list/reading_list_entry.h"

class ReadingListModel;
class ReadingListStoreDelegate;

namespace syncer {
class ModelTypeSyncBridge;
}

// Interface for a persistence layer for reading list.
// All interface methods have to be called on main thread.
class ReadingListModelStorage {
 public:
  class ScopedBatchUpdate;

  // Sets the model the Storage is backing.
  // This will trigger store initalization and load persistent entries.
  virtual void SetReadingListModel(ReadingListModel* model,
                                   ReadingListStoreDelegate* delegate) = 0;

  // Returns the class responsible for handling sync messages.
  virtual syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() = 0;

  // Starts a transaction. All Save/Remove entry will be delayed until the
  // transaction is commited.
  // Multiple transaction can be started at the same time. Commit will happen
  // when the last transaction is commited.
  // Returns a scoped batch update object that should be retained while the
  // batch update is performed. Deallocating this object will inform model that
  // the batch update has completed.
  virtual std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() = 0;

  // Saves or updates an entry. If the entry is not yet in the database, it is
  // created.
  virtual void SaveEntry(const ReadingListEntry& entry, bool read) = 0;

  // Removed an entry from the storage.
  virtual void RemoveEntry(const ReadingListEntry& entry) = 0;

  class ScopedBatchUpdate {
   public:
    virtual ~ScopedBatchUpdate() {}
  };
};

#endif  // COMPONENTS_READING_LIST_READING_LIST_MODEL_STORAGE_H_
