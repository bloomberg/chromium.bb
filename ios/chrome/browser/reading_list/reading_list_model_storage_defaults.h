// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_STORAGE_DEFAULTS_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_STORAGE_DEFAULTS_H_

#include "ios/chrome/browser/reading_list/reading_list_entry.h"
#include "ios/chrome/browser/reading_list/reading_list_model_storage.h"

class ReadingListModel;

// Implementation of ReadingListModelStorage that stores reading list items in
// the system user defaults.
class ReadingListModelStorageDefaults : public ReadingListModelStorage {
 public:
  std::vector<ReadingListEntry> LoadPersistentReadList() override;
  std::vector<ReadingListEntry> LoadPersistentUnreadList() override;
  bool LoadPersistentHasUnseen() override;

  void SavePersistentReadList(
      const std::vector<ReadingListEntry>& read) override;
  void SavePersistentUnreadList(
      const std::vector<ReadingListEntry>& unread) override;
  void SavePersistentHasUnseen(bool has_unseen) override;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_STORAGE_DEFAULTS_H_