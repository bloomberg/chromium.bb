// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_IMPL_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_IMPL_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/reading_list/reading_list_entry.h"
#include "ios/chrome/browser/reading_list/reading_list_model.h"

class ReadingListModelStorage;

// Concrete implementation of a reading list model using in memory lists.
class ReadingListModelImpl : public ReadingListModel, public KeyedService {
 public:
  // Initialize a ReadingListModelImpl to load and save data in
  // |persistence_layer|.
  ReadingListModelImpl(std::unique_ptr<ReadingListModelStorage> storage_layer);

  // Initialize a ReadingListModelImpl without persistence. Data will not be
  // persistent across sessions.
  ReadingListModelImpl();

  ~ReadingListModelImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // ReadingListModel implementation.
  bool loaded() const override;

  size_t unread_size() const override;
  size_t read_size() const override;

  bool HasUnseenEntries() const override;
  void ResetUnseenEntries() override;

  const ReadingListEntry& GetUnreadEntryAtIndex(size_t index) const override;
  const ReadingListEntry& GetReadEntryAtIndex(size_t index) const override;

  const ReadingListEntry* GetEntryFromURL(const GURL& gurl) const override;

  bool CallbackEntryURL(
      const GURL& url,
      base::Callback<void(const ReadingListEntry&)> callback) const override;

  void RemoveEntryByUrl(const GURL& url) override;

  const ReadingListEntry& AddEntry(const GURL& url,
                                   const std::string& title) override;

  void MarkReadByURL(const GURL& url) override;
  void MarkUnreadByURL(const GURL& url) override;

  void SetEntryTitle(const GURL& url, const std::string& title) override;
  void SetEntryDistilledURL(const GURL& url,
                            const GURL& distilled_url) override;
  void SetEntryDistilledState(
      const GURL& url,
      ReadingListEntry::DistillationState state) override;

 protected:
  void EndBatchUpdates() override;

 private:
  typedef std::vector<ReadingListEntry> ReadingListEntries;

  ReadingListEntries unread_;
  ReadingListEntries read_;
  std::unique_ptr<ReadingListModelStorage> storageLayer_;
  bool hasUnseen_;
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListModelImpl);
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_MEMORY_H_
