// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_READING_LIST_STORE_DELEGATE_H_
#define COMPONENTS_READING_LIST_READING_LIST_STORE_DELEGATE_H_

class ReadingListEntry;

// The delegate to handle callbacks from the ReadingListStore.
class ReadingListStoreDelegate {
 public:
  // These three mathods handle callbacks from a ReadingListStore.
  virtual void StoreLoaded(std::unique_ptr<ReadingListEntries> unread,
                           std::unique_ptr<ReadingListEntries> read) = 0;
  // Handle sync events.
  // Called to add a new entry to the model.
  // |entry| must not already exist in the model.
  virtual void SyncAddEntry(std::unique_ptr<ReadingListEntry> entry,
                            bool read) = 0;

  // Called to merge a sync entry with a local entry in the model.
  // A local entry with the same URL must exist in the local store and have an
  // older UpdateTime.
  // Return a pointer to the merged entry.
  virtual ReadingListEntry* SyncMergeEntry(
      std::unique_ptr<ReadingListEntry> entry,
      bool read) = 0;

  // Called to remove an entry to the model.
  virtual void SyncRemoveEntry(const GURL& url) = 0;
};

#endif  // COMPONENTS_READING_LIST_READING_LIST_STORE_DELEGATE_H_
