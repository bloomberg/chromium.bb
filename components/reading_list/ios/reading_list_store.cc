// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/reading_list_store.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/reading_list/ios/proto/reading_list.pb.h"
#include "components/reading_list/ios/reading_list_model_impl.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/accumulating_metadata_change_list.h"
#include "components/sync/protocol/model_type_state.pb.h"

ReadingListStore::ReadingListStore(
    StoreFactoryFunction create_store_callback,
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeSyncBridge(change_processor_factory, syncer::READING_LIST),
      create_store_callback_(create_store_callback),
      pending_transaction_count_(0) {}

ReadingListStore::~ReadingListStore() {
  DCHECK(pending_transaction_count_ == 0);
}

void ReadingListStore::SetReadingListModel(ReadingListModel* model,
                                           ReadingListStoreDelegate* delegate) {
  DCHECK(CalledOnValidThread());
  model_ = model;
  delegate_ = delegate;
  create_store_callback_.Run(
      base::Bind(&ReadingListStore::OnStoreCreated, base::AsWeakPtr(this)));
}

std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate>
ReadingListStore::EnsureBatchCreated() {
  return base::WrapUnique<ReadingListModelStorage::ScopedBatchUpdate>(
      new ScopedBatchUpdate(this));
}

ReadingListStore::ScopedBatchUpdate::ScopedBatchUpdate(ReadingListStore* store)
    : store_(store) {
  store_->BeginTransaction();
}

ReadingListStore::ScopedBatchUpdate::~ScopedBatchUpdate() {
  store_->CommitTransaction();
}

void ReadingListStore::BeginTransaction() {
  DCHECK(CalledOnValidThread());
  pending_transaction_count_++;
  if (pending_transaction_count_ == 1) {
    batch_ = store_->CreateWriteBatch();
  }
}

void ReadingListStore::CommitTransaction() {
  DCHECK(CalledOnValidThread());
  pending_transaction_count_--;
  if (pending_transaction_count_ == 0) {
    store_->CommitWriteBatch(
        std::move(batch_),
        base::Bind(&ReadingListStore::OnDatabaseSave, base::AsWeakPtr(this)));
    batch_.reset();
  }
}

void ReadingListStore::SaveEntry(const ReadingListEntry& entry) {
  DCHECK(CalledOnValidThread());
  auto token = EnsureBatchCreated();

  std::unique_ptr<reading_list::ReadingListLocal> pb_entry =
      entry.AsReadingListLocal();

  batch_->WriteData(entry.URL().spec(), pb_entry->SerializeAsString());

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }
  std::unique_ptr<sync_pb::ReadingListSpecifics> pb_entry_sync =
      entry.AsReadingListSpecifics();

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  std::unique_ptr<syncer::EntityData> entity_data(new syncer::EntityData());
  *entity_data->specifics.mutable_reading_list() = *pb_entry_sync;
  entity_data->non_unique_name = pb_entry_sync->entry_id();

  change_processor()->Put(entry.URL().spec(), std::move(entity_data),
                          metadata_change_list.get());
  batch_->TransferMetadataChanges(std::move(metadata_change_list));
}

void ReadingListStore::RemoveEntry(const ReadingListEntry& entry) {
  DCHECK(CalledOnValidThread());
  auto token = EnsureBatchCreated();

  batch_->DeleteData(entry.URL().spec());
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  change_processor()->Delete(entry.URL().spec(), metadata_change_list.get());
  batch_->TransferMetadataChanges(std::move(metadata_change_list));
}

void ReadingListStore::OnDatabaseLoad(
    syncer::ModelTypeStore::Result result,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries) {
  DCHECK(CalledOnValidThread());
  if (result != syncer::ModelTypeStore::Result::SUCCESS) {
    change_processor()->OnMetadataLoaded(
        change_processor()->CreateAndUploadError(
            FROM_HERE, "Cannot load Reading List Database."),
        nullptr);
    return;
  }
  auto loaded_entries =
      base::MakeUnique<ReadingListStoreDelegate::ReadingListEntries>();

  for (const syncer::ModelTypeStore::Record& r : *entries.get()) {
    reading_list::ReadingListLocal proto;
    if (!proto.ParseFromString(r.value)) {
      continue;
      // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
      // failure.
    }

    std::unique_ptr<ReadingListEntry> entry(
        ReadingListEntry::FromReadingListLocal(proto));
    if (!entry) {
      continue;
    }
    GURL url = entry->URL();
    DCHECK(!loaded_entries->count(url));
    loaded_entries->insert(std::make_pair(url, std::move(*entry)));
  }

  delegate_->StoreLoaded(std::move(loaded_entries));

  store_->ReadAllMetadata(
      base::Bind(&ReadingListStore::OnReadAllMetadata, base::AsWeakPtr(this)));
}

void ReadingListStore::OnReadAllMetadata(
    syncer::SyncError sync_error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK(CalledOnValidThread());
  change_processor()->OnMetadataLoaded(sync_error, std::move(metadata_batch));
}

void ReadingListStore::OnDatabaseSave(syncer::ModelTypeStore::Result result) {
  return;
}

void ReadingListStore::OnStoreCreated(
    syncer::ModelTypeStore::Result result,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK(CalledOnValidThread());
  if (result != syncer::ModelTypeStore::Result::SUCCESS) {
    // TODO(crbug.com/664926): handle store creation error.
    return;
  }
  store_ = std::move(store);
  store_->ReadAllData(
      base::Bind(&ReadingListStore::OnDatabaseLoad, base::AsWeakPtr(this)));
  return;
}

syncer::ModelTypeSyncBridge* ReadingListStore::GetModelTypeSyncBridge() {
  return this;
}

// Creates an object used to communicate changes in the sync metadata to the
// model type store.
std::unique_ptr<syncer::MetadataChangeList>
ReadingListStore::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

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
syncer::SyncError ReadingListStore::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityDataMap entity_data_map) {
  DCHECK(CalledOnValidThread());
  auto token = EnsureBatchCreated();
  // Keep track of the last update of each item.
  std::set<std::string> synced_entries;
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
      model_batch_updates = model_->BeginBatchUpdates();

  // Merge sync to local data.
  for (const auto& kv : entity_data_map) {
    synced_entries.insert(kv.first);
    const sync_pb::ReadingListSpecifics& specifics =
        kv.second.value().specifics.reading_list();
    // Deserialize entry.
    std::unique_ptr<ReadingListEntry> entry(
        ReadingListEntry::FromReadingListSpecifics(specifics));

    const ReadingListEntry* existing_entry =
        model_->GetEntryByURL(entry->URL());

    if (!existing_entry) {
      // This entry is new. Add it to the store and model.
      // Convert to local store format and write to store.
      std::unique_ptr<reading_list::ReadingListLocal> entry_pb =
          entry->AsReadingListLocal();
      batch_->WriteData(entry->URL().spec(), entry_pb->SerializeAsString());

      // Notify model about updated entry.
      delegate_->SyncAddEntry(std::move(entry));
    } else if (existing_entry->UpdateTime() < entry->UpdateTime()) {
      // The entry from sync is more recent.
      // Merge the local data to it and store it.
      ReadingListEntry* merged_entry =
          delegate_->SyncMergeEntry(std::move(entry));

      // Write to the store.
      std::unique_ptr<reading_list::ReadingListLocal> entry_local_pb =
          merged_entry->AsReadingListLocal();
      batch_->WriteData(entry->URL().spec(),
                        entry_local_pb->SerializeAsString());

      // Send to sync
      std::unique_ptr<sync_pb::ReadingListSpecifics> entry_sync_pb =
          merged_entry->AsReadingListSpecifics();
      auto entity_data = base::MakeUnique<syncer::EntityData>();
      *(entity_data->specifics.mutable_reading_list()) = *entry_sync_pb;
      entity_data->non_unique_name = entry_sync_pb->entry_id();

      // TODO(crbug.com/666232): Investigate if there is a risk of sync
      // ping-pong.
      change_processor()->Put(entry_sync_pb->entry_id(), std::move(entity_data),
                              metadata_change_list.get());

    } else {
      // The entry from sync is out of date.
      // Send back the local more recent entry.
      // No need to update
      std::unique_ptr<sync_pb::ReadingListSpecifics> entry_pb =
          existing_entry->AsReadingListSpecifics();
      auto entity_data = base::MakeUnique<syncer::EntityData>();
      *(entity_data->specifics.mutable_reading_list()) = *entry_pb;
      entity_data->non_unique_name = entry_pb->entry_id();

      change_processor()->Put(entry_pb->entry_id(), std::move(entity_data),
                              metadata_change_list.get());
    }
  }

  // Commit local only entries to server.
  for (const auto& url : model_->Keys()) {
    const ReadingListEntry* entry = model_->GetEntryByURL(url);
    if (synced_entries.count(url.spec())) {
      // Entry already exists and has been merged above.
      continue;
    }

    // Local entry has later timestamp. It should be committed to server.
    std::unique_ptr<sync_pb::ReadingListSpecifics> entry_pb =
        entry->AsReadingListSpecifics();

    auto entity_data = base::MakeUnique<syncer::EntityData>();
    *(entity_data->specifics.mutable_reading_list()) = *entry_pb;
    entity_data->non_unique_name = entry_pb->entry_id();

    change_processor()->Put(entry_pb->entry_id(), std::move(entity_data),
                            metadata_change_list.get());
  }
  batch_->TransferMetadataChanges(std::move(metadata_change_list));

  return syncer::SyncError();
}

// Apply changes from the sync server locally.
// Please note that |entity_changes| might have fewer entries than
// |metadata_change_list| in case when some of the data changes are filtered
// out, or even be empty in case when a commit confirmation is processed and
// only the metadata needs to persisted.
syncer::SyncError ReadingListStore::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK(CalledOnValidThread());
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> batch =
      model_->BeginBatchUpdates();
  auto token = EnsureBatchCreated();

  for (syncer::EntityChange& change : entity_changes) {
    if (change.type() == syncer::EntityChange::ACTION_DELETE) {
      batch_->DeleteData(change.storage_key());
      // Need to notify model that entry is deleted.
      delegate_->SyncRemoveEntry(GURL(change.storage_key()));
    } else {
      // Deserialize entry.
      const sync_pb::ReadingListSpecifics& specifics =
          change.data().specifics.reading_list();
      std::unique_ptr<ReadingListEntry> entry(
          ReadingListEntry::FromReadingListSpecifics(specifics));

      const ReadingListEntry* existing_entry =
          model_->GetEntryByURL(entry->URL());

      if (!existing_entry) {
        // This entry is new. Add it to the store and model.
        // Convert to local store format and write to store.
        std::unique_ptr<reading_list::ReadingListLocal> entry_pb =
            entry->AsReadingListLocal();
        batch_->WriteData(entry->URL().spec(), entry_pb->SerializeAsString());

        // Notify model about updated entry.
        delegate_->SyncAddEntry(std::move(entry));
      } else if (existing_entry->UpdateTime() < entry->UpdateTime()) {
        // The entry from sync is more recent.
        // Merge the local data to it and store it.
        ReadingListEntry* merged_entry =
            delegate_->SyncMergeEntry(std::move(entry));

        // Write to the store.
        std::unique_ptr<reading_list::ReadingListLocal> entry_local_pb =
            merged_entry->AsReadingListLocal();
        batch_->WriteData(merged_entry->URL().spec(),
                          entry_local_pb->SerializeAsString());

        // Send to sync
        std::unique_ptr<sync_pb::ReadingListSpecifics> entry_sync_pb =
            merged_entry->AsReadingListSpecifics();
        auto entity_data = base::MakeUnique<syncer::EntityData>();
        *(entity_data->specifics.mutable_reading_list()) = *entry_sync_pb;
        entity_data->non_unique_name = entry_sync_pb->entry_id();

        // TODO(crbug.com/666232): Investigate if there is a risk of sync
        // ping-pong.
        change_processor()->Put(entry_sync_pb->entry_id(),
                                std::move(entity_data),
                                metadata_change_list.get());

      } else {
        // The entry from sync is out of date.
        // Send back the local more recent entry.
        // No need to update
        std::unique_ptr<sync_pb::ReadingListSpecifics> entry_pb =
            existing_entry->AsReadingListSpecifics();
        auto entity_data = base::MakeUnique<syncer::EntityData>();
        *(entity_data->specifics.mutable_reading_list()) = *entry_pb;
        entity_data->non_unique_name = entry_pb->entry_id();

        change_processor()->Put(entry_pb->entry_id(), std::move(entity_data),
                                metadata_change_list.get());
      }
    }
  }

  batch_->TransferMetadataChanges(std::move(metadata_change_list));
  return syncer::SyncError();
}

void ReadingListStore::GetData(StorageKeyList storage_keys,
                               DataCallback callback) {
  DCHECK(CalledOnValidThread());
  auto batch = base::MakeUnique<syncer::MutableDataBatch>();
  for (const std::string& url_string : storage_keys) {
    const ReadingListEntry* entry = model_->GetEntryByURL(GURL(url_string));
    if (entry) {
      AddEntryToBatch(batch.get(), *entry);
    }
  }

  callback.Run(syncer::SyncError(), std::move(batch));
}

void ReadingListStore::GetAllData(DataCallback callback) {
  DCHECK(CalledOnValidThread());
  auto batch = base::MakeUnique<syncer::MutableDataBatch>();

  for (const auto& url : model_->Keys()) {
    const ReadingListEntry* entry = model_->GetEntryByURL(GURL(url));
    AddEntryToBatch(batch.get(), *entry);
  }

  callback.Run(syncer::SyncError(), std::move(batch));
}

void ReadingListStore::AddEntryToBatch(syncer::MutableDataBatch* batch,
                                       const ReadingListEntry& entry) {
  DCHECK(CalledOnValidThread());
  std::unique_ptr<sync_pb::ReadingListSpecifics> entry_pb =
      entry.AsReadingListSpecifics();

  std::unique_ptr<syncer::EntityData> entity_data(new syncer::EntityData());
  *(entity_data->specifics.mutable_reading_list()) = *entry_pb;
  entity_data->non_unique_name = entry_pb->entry_id();

  batch->Put(entry_pb->entry_id(), std::move(entity_data));
}

// Get or generate a client tag for |entity_data|. This must be the same tag
// that was/would have been generated in the SyncableService/Directory world
// for backward compatibility with pre-USS clients. The only time this
// theoretically needs to be called is on the creation of local data, however
// it is also used to verify the hash of remote data. If a data type was never
// launched pre-USS, then method does not need to be different from
// GetStorageKey().
std::string ReadingListStore::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(CalledOnValidThread());
  return GetStorageKey(entity_data);
}

// Get or generate a storage key for |entity_data|. This will only ever be
// called once when first encountering a remote entity. Local changes will
// provide their storage keys directly to Put instead of using this method.
// Theoretically this function doesn't need to be stable across multiple calls
// on the same or different clients, but to keep things simple, it probably
// should be.
std::string ReadingListStore::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(CalledOnValidThread());
  return entity_data.specifics.reading_list().entry_id();
}
