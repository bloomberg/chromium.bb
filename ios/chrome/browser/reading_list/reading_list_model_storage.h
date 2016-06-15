// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_STORAGE_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_STORAGE_H_

#include <vector>

#include "ios/chrome/browser/reading_list/reading_list_entry.h"

class ReadingListModel;

// Interface for a persistence layer for reading list.
// All interface methods have to be called on main thread.
class ReadingListModelStorage {
 public:
  virtual std::vector<ReadingListEntry> LoadPersistentReadList() = 0;
  virtual std::vector<ReadingListEntry> LoadPersistentUnreadList() = 0;
  virtual bool LoadPersistentHasUnseen() = 0;

  virtual void SavePersistentReadList(
      const std::vector<ReadingListEntry>& read) = 0;
  virtual void SavePersistentUnreadList(
      const std::vector<ReadingListEntry>& unread) = 0;
  virtual void SavePersistentHasUnseen(bool has_unseen) = 0;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_STORAGE_H_
