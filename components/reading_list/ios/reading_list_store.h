// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_IOS_READING_LIST_STORE_H_
#define COMPONENTS_READING_LIST_IOS_READING_LIST_STORE_H_

#include "base/threading/non_thread_safe.h"
#include "components/reading_list/ios/reading_list_model_storage.h"
#include "components/reading_list/ios/reading_list_store_delegate.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class MutableDataBatch;
}

class ReadingListModel;

// A ReadingListModelStorage storing and syncing data in protobufs.
class ReadingListStore : public syncer::ModelTypeSyncBridge,
                         public ReadingListModelStorage,
                         public base::NonThreadSafe {
  using StoreFactoryFunction = base::Callback<void(
      const syncer::ModelTypeStore::InitCallback& callback)>;

 public:
  ReadingListStore(StoreFactoryFunction create_store_callback,
                   const ChangeProcessorFactory& change_processor_factory);
  ~ReadingListStore() override;

  std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() override;

  // ReadingListModelStorage implementation
  void SetReadingListModel(ReadingListModel* model,
                           ReadingListStoreDelegate* delegate) override;

  void SaveEntry(const ReadingListEntry& entry) override;
  void RemoveEntry(const ReadingListEntry& entry) override;

  // ReadingListModelStorage implementation.
  syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() override;

  // Creates an object used to communicate changes in the sync metadata to the
  // model type store.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;

  // Perform the initial merge between local and sync data. This should only be
  // called when a data type is first enabled to start syncing, and there is no
  // sync metadata. Best effort should be made to match local and sync data. The
  // keys in the |entity_data_map| will have been created via GetClientTag(...),
  // and if a local and sync data should match/merge but disagree on tags, the
  // service should use the sync data's tag. Any local pieces of data that are
  // not present in sync should immediately be Put(...) to the processor before
  // returning. The same MetadataChangeList that was passed into this function
  // can be passed to Put(...) calls. Delete(...) can also be called but should
  // not be needed for most model types. Durable storage writes, if not able to
  // combine all change atomically, should save the metadata after the data
  // changes, so that this merge will be re-driven by sync if is not completely
  // saved during the current run.
  syncer::SyncError MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityDataMap entity_data_map) override;

  // Apply changes from the sync server locally.
  // Please note that |entity_changes| might have fewer entries than
  // |metadata_change_list| in case when some of the data changes are filtered
  // out, or even be empty in case when a commit confirmation is processed and
  // only the metadata needs to persisted.
  syncer::SyncError ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;

  // Asynchronously retrieve the corresponding sync data for |storage_keys|.
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;

  // Asynchronously retrieve all of the local sync data.
  void GetAllData(DataCallback callback) override;

  // Get or generate a client tag for |entity_data|. This must be the same tag
  // that was/would have been generated in the SyncableService/Directory world
  // for backward compatibility with pre-USS clients. The only time this
  // theoretically needs to be called is on the creation of local data, however
  // it is also used to verify the hash of remote data. If a data type was never
  // launched pre-USS, then method does not need to be different from
  // GetStorageKey().
  std::string GetClientTag(const syncer::EntityData& entity_data) override;

  // Get or generate a storage key for |entity_data|. This will only ever be
  // called once when first encountering a remote entity. Local changes will
  // provide their storage keys directly to Put instead of using this method.
  // Theoretically this function doesn't need to be stable across multiple calls
  // on the same or different clients, but to keep things simple, it probably
  // should be.
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // Methods used as callbacks given to DataTypeStore.
  void OnStoreCreated(syncer::ModelTypeStore::Result result,
                      std::unique_ptr<syncer::ModelTypeStore> store);

  class ScopedBatchUpdate : public ReadingListModelStorage::ScopedBatchUpdate {
   public:
    explicit ScopedBatchUpdate(ReadingListStore* store);

    ~ScopedBatchUpdate() override;

   private:
    ReadingListStore* store_;

    DISALLOW_COPY_AND_ASSIGN(ScopedBatchUpdate);
  };

 private:
  void BeginTransaction();
  void CommitTransaction();
  // Callbacks needed for the database handling.
  void OnDatabaseLoad(
      syncer::ModelTypeStore::Result result,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries);
  void OnDatabaseSave(syncer::ModelTypeStore::Result result);
  void OnReadAllMetadata(syncer::SyncError sync_error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void AddEntryToBatch(syncer::MutableDataBatch* batch,
                       const ReadingListEntry& entry);

  std::unique_ptr<syncer::ModelTypeStore> store_;
  ReadingListModel* model_;
  ReadingListStoreDelegate* delegate_;
  StoreFactoryFunction create_store_callback_;

  int pending_transaction_count_;
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch_;
};

#endif  // COMPONENTS_READING_LIST_IOS_READING_LIST_STORE_H_
