// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_IOS_READING_LIST_MODEL_IMPL_H_
#define COMPONENTS_READING_LIST_IOS_READING_LIST_MODEL_IMPL_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/ios/reading_list_entry.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "components/reading_list/ios/reading_list_model_storage.h"
#include "components/reading_list/ios/reading_list_store_delegate.h"

class PrefService;

// Concrete implementation of a reading list model using in memory lists.
class ReadingListModelImpl : public ReadingListModel,
                             public ReadingListStoreDelegate,
                             public KeyedService {
 public:
  // Initialize a ReadingListModelImpl to load and save data in
  // |persistence_layer|.
  ReadingListModelImpl(std::unique_ptr<ReadingListModelStorage> storage_layer,
                       PrefService* pref_service);

  // Initialize a ReadingListModelImpl without persistence. Data will not be
  // persistent across sessions.
  ReadingListModelImpl();

  syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() override;

  ~ReadingListModelImpl() override;

  void StoreLoaded(std::unique_ptr<ReadingListEntries> unread,
                   std::unique_ptr<ReadingListEntries> read) override;

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

  const ReadingListEntry* GetEntryFromURL(const GURL& gurl,
                                          bool* read) const override;

  bool CallbackEntryURL(
      const GURL& url,
      base::Callback<void(const ReadingListEntry&)> callback) const override;

  void RemoveEntryByURL(const GURL& url) override;

  const ReadingListEntry& AddEntry(const GURL& url,
                                   const std::string& title) override;

  void MarkReadByURL(const GURL& url) override;
  void MarkUnreadByURL(const GURL& url) override;

  void SetEntryTitle(const GURL& url, const std::string& title) override;
  void SetEntryDistilledPath(const GURL& url,
                             const base::FilePath& distilled_path) override;
  void SetEntryDistilledState(
      const GURL& url,
      ReadingListEntry::DistillationState state) override;

  void SyncAddEntry(std::unique_ptr<ReadingListEntry> entry,
                    bool read) override;
  ReadingListEntry* SyncMergeEntry(std::unique_ptr<ReadingListEntry> entry,
                                   bool read) override;
  void SyncRemoveEntry(const GURL& url) override;

  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
  CreateBatchToken() override;

  // Helper class that is used to scope batch updates.
  class ScopedReadingListBatchUpdate
      : public ReadingListModel::ScopedReadingListBatchUpdate {
   public:
    explicit ScopedReadingListBatchUpdate(ReadingListModelImpl* model);

    ~ScopedReadingListBatchUpdate() override;

   private:
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> storage_token_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadingListBatchUpdate);
  };

 protected:
  void EnteringBatchUpdates() override;
  void LeavingBatchUpdates() override;

 private:
  // Sets/Loads the pref flag that indicate if some entries have never been seen
  // since being added to the store.
  void SetPersistentHasUnseen(bool has_unseen);
  bool GetPersistentHasUnseen();

  // Returns a mutable pointer to the entry with URL |gurl|. Return nullptr if
  // no entry is found. If an entry is found, |read| is set to the read status
  // of the entry.
  ReadingListEntry* GetMutableEntryFromURL(const GURL& gurl, bool* read) const;

  // Sorts the entries in |read_| and |unread_| according to their |UpdateTime|.
  void SortEntries();

  // Returns the |storage_layer_| of the model.
  ReadingListModelStorage* StorageLayer();

  // Remove |entry| from the |entries| vector and calls the Move notifications
  // on observers.
  void MoveEntryFrom(ReadingListEntries* entries,
                     const ReadingListEntry& entry,
                     bool read);

  // Remove entry |url| and propagate to store if |from_sync| is false.
  void RemoveEntryByURLImpl(const GURL& url, bool from_sync);

  std::unique_ptr<ReadingListEntries> unread_;
  std::unique_ptr<ReadingListEntries> read_;
  std::unique_ptr<ReadingListModelStorage> storage_layer_;
  PrefService* pref_service_;
  bool has_unseen_;
  bool loaded_;
  base::WeakPtrFactory<ReadingListModelImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ReadingListModelImpl);
};

#endif  // COMPONENTS_READING_LIST_IOS_READING_LIST_MODEL_IMPL_H_
