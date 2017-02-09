// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace chromeos {

namespace {

using Result = syncer::ModelTypeStore::Result;

using syncer::ConflictResolution;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::ModelTypeChangeProcessor;
using syncer::ModelTypeStore;
using syncer::MetadataChangeList;

std::unique_ptr<EntityData> CopyToEntityData(
    const sync_pb::PrinterSpecifics& specifics) {
  auto entity_data = base::MakeUnique<EntityData>();
  *entity_data->specifics.mutable_printer() = specifics;
  entity_data->non_unique_name =
      specifics.display_name().empty() ? "PRINTER" : specifics.display_name();
  return entity_data;
}

// Store |specifics| locally in |data| and record change in |batch|.
void StoreSpecifics(
    std::unique_ptr<sync_pb::PrinterSpecifics> specifics,
    std::map<std::string, std::unique_ptr<sync_pb::PrinterSpecifics>>* data,
    ModelTypeStore::WriteBatch* batch) {
  const std::string id = specifics->id();
  batch->WriteData(id, specifics->SerializeAsString());
  (*data)[id] = std::move(specifics);
}

// Remove the specific with |id| from |data| and update |batch| with the change.
bool DeleteSpecifics(
    const std::string& id,
    std::map<std::string, std::unique_ptr<sync_pb::PrinterSpecifics>>* data,
    ModelTypeStore::WriteBatch* batch) {
  auto iter = data->find(id);
  if (iter != data->end()) {
    batch->DeleteData(id);
    data->erase(iter);
    return true;
  }

  return false;
}

}  // namespace

// Delegate class which helps to manage the ModelTypeStore.
class PrintersSyncBridge::StoreProxy {
 public:
  StoreProxy(PrintersSyncBridge* owner,
             const syncer::ModelTypeStoreFactory& callback)
      : owner_(owner), weak_ptr_factory_(this) {
    callback.Run(base::Bind(&StoreProxy::OnStoreCreated,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // Returns true if the store has been initialized.
  bool Ready() { return store_.get() != nullptr; }

  // Returns a new WriteBatch.
  std::unique_ptr<ModelTypeStore::WriteBatch> CreateWriteBatch() {
    CHECK(store_);
    return store_->CreateWriteBatch();
  }

  // Commits writes to the database and updates metadata.
  void Commit(std::unique_ptr<ModelTypeStore::WriteBatch> batch) {
    CHECK(store_);
    store_->CommitWriteBatch(
        std::move(batch),
        base::Bind(&StoreProxy::OnCommit, weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Callback for ModelTypeStore initialization.
  void OnStoreCreated(Result result, std::unique_ptr<ModelTypeStore> store) {
    if (result == Result::SUCCESS) {
      store_ = std::move(store);
      store_->ReadAllData(base::Bind(&StoreProxy::OnReadAllData,
                                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      owner_->change_processor()->ReportError(
          FROM_HERE, "ModelTypeStore creation failed.");
    }
  }

  void OnReadAllData(Result result,
                     std::unique_ptr<ModelTypeStore::RecordList> record_list) {
    if (result != Result::SUCCESS) {
      owner_->change_processor()->ReportError(FROM_HERE,
                                              "Initial load of data failed");
      return;
    }

    bool error = false;
    for (const ModelTypeStore::Record& r : *record_list) {
      auto specifics = base::MakeUnique<sync_pb::PrinterSpecifics>();
      if (specifics->ParseFromString(r.value)) {
        owner_->all_data_[specifics->id()] = std::move(specifics);
      } else {
        error = true;
      }
    }

    if (error) {
      owner_->change_processor()->ReportError(
          FROM_HERE, "Failed to deserialize all specifics.");
      return;
    }

    // Data loaded.  Load metadata.
    store_->ReadAllMetadata(base::Bind(&StoreProxy::OnReadAllMetadata,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Callback to handle commit errors.
  void OnCommit(ModelTypeStore::Result result) {
    if (result != Result::SUCCESS) {
      LOG(WARNING) << "Failed to commit operation to store";
      owner_->change_processor()->ReportError(FROM_HERE,
                                              "Failed to commit to store");
    }
  }

  void OnReadAllMetadata(
      base::Optional<syncer::ModelError> error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
    if (error) {
      owner_->change_processor()->ReportError(error.value());
      return;
    }

    owner_->change_processor()->ModelReadyToSync(std::move(metadata_batch));
  }

  PrintersSyncBridge* owner_;

  std::unique_ptr<ModelTypeStore> store_;
  base::WeakPtrFactory<StoreProxy> weak_ptr_factory_;
};

PrintersSyncBridge::PrintersSyncBridge(
    const syncer::ModelTypeStoreFactory& callback,
    const base::RepeatingClosure& error_callback)
    : ModelTypeSyncBridge(base::BindRepeating(&ModelTypeChangeProcessor::Create,
                                              error_callback),
                          syncer::PRINTERS),
      store_delegate_(base::MakeUnique<StoreProxy>(this, callback)) {}

PrintersSyncBridge::~PrintersSyncBridge() {}

std::unique_ptr<MetadataChangeList>
PrintersSyncBridge::CreateMetadataChangeList() {
  return ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

base::Optional<syncer::ModelError> PrintersSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityDataMap entity_data_map) {
  DCHECK(change_processor()->IsTrackingMetadata());

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_delegate_->CreateWriteBatch();
  for (const auto& entry : entity_data_map) {
    const sync_pb::PrinterSpecifics& specifics =
        entry.second.value().specifics.printer();

    DCHECK_EQ(entry.first, specifics.id());

    // Write the update to local storage even if we already have it.
    StoreSpecifics(base::MakeUnique<sync_pb::PrinterSpecifics>(specifics),
                   &all_data_, batch.get());
  }

  // Inform the change processor of the new local entities and generate
  // appropriate metadata.
  for (const auto& entry : all_data_) {
    const std::string& local_entity_id = entry.first;
    if (!base::ContainsKey(entity_data_map, local_entity_id)) {
      // Only local objects which were not updated are uploaded.  Objects for
      // which there was a remote copy are overwritten.
      change_processor()->Put(local_entity_id, CopyToEntityData(*entry.second),
                              metadata_change_list.get());
    }
  }

  batch->TransferMetadataChanges(std::move(metadata_change_list));
  store_delegate_->Commit(std::move(batch));
  return {};
}

base::Optional<syncer::ModelError> PrintersSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_delegate_->CreateWriteBatch();
  // For all the entities from the server, apply changes.
  for (const EntityChange& change : entity_changes) {
    // We register the entity's storage key as our printer ids since they're
    // globally unique.
    const std::string& id = change.storage_key();
    if (change.type() == EntityChange::ACTION_DELETE) {
      // Server says delete, try to remove locally.
      DeleteSpecifics(id, &all_data_, batch.get());
    } else {
      // Server says update, overwrite whatever is local.  Conflict resolution
      // guarantees that this will be the newest version of the object.
      const sync_pb::PrinterSpecifics& specifics =
          change.data().specifics.printer();
      DCHECK_EQ(id, specifics.id());
      StoreSpecifics(base::MakeUnique<sync_pb::PrinterSpecifics>(specifics),
                     &all_data_, batch.get());
    }
  }

  // Update the local database with metadata for the incoming changes.
  batch->TransferMetadataChanges(std::move(metadata_change_list));

  store_delegate_->Commit(std::move(batch));
  return {};
}

void PrintersSyncBridge::GetData(StorageKeyList storage_keys,
                                 DataCallback callback) {
  auto batch = base::MakeUnique<syncer::MutableDataBatch>();
  for (const auto& key : storage_keys) {
    auto found = all_data_.find(key);
    if (found != all_data_.end()) {
      batch->Put(key, CopyToEntityData(*found->second));
    }
  }

  callback.Run(std::move(batch));
}

void PrintersSyncBridge::GetAllData(DataCallback callback) {
  auto batch = base::MakeUnique<syncer::MutableDataBatch>();
  for (const auto& entry : all_data_) {
    batch->Put(entry.first, CopyToEntityData(*entry.second));
  }

  callback.Run(std::move(batch));
}

std::string PrintersSyncBridge::GetClientTag(const EntityData& entity_data) {
  // Printers were never synced prior to USS so this can match GetStorageKey.
  return GetStorageKey(entity_data);
}

std::string PrintersSyncBridge::GetStorageKey(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_printer());
  return entity_data.specifics.printer().id();
}

// Picks the entity with the most recent updated time as the canonical version.
ConflictResolution PrintersSyncBridge::ResolveConflict(
    const EntityData& local_data,
    const EntityData& remote_data) const {
  DCHECK(local_data.specifics.has_printer());
  DCHECK(remote_data.specifics.has_printer());

  const sync_pb::PrinterSpecifics& local_printer =
      local_data.specifics.printer();
  const sync_pb::PrinterSpecifics& remote_printer =
      remote_data.specifics.printer();

  if (local_printer.updated_timestamp() > remote_printer.updated_timestamp()) {
    return ConflictResolution::UseLocal();
  }

  return ConflictResolution::UseRemote();
}

void PrintersSyncBridge::AddPrinter(
    std::unique_ptr<sync_pb::PrinterSpecifics> printer) {
  printer->set_updated_timestamp(base::Time::Now().ToJavaTime());

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_delegate_->CreateWriteBatch();
  if (change_processor()->IsTrackingMetadata()) {
    change_processor()->Put(printer->id(), CopyToEntityData(*printer),
                            batch->GetMetadataChangeList());
  }

  StoreSpecifics(std::move(printer), &all_data_, batch.get());
  store_delegate_->Commit(std::move(batch));
}

bool PrintersSyncBridge::RemovePrinter(const std::string& id) {
  DCHECK(store_delegate_->Ready());

  // Remove from local cache.
  if (all_data_.erase(id) == 0) {
    LOG(WARNING) << "Could not find printer" << id;
    return false;
  }

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_delegate_->CreateWriteBatch();
  if (change_processor()->IsTrackingMetadata()) {
    change_processor()->Delete(id, batch->GetMetadataChangeList());
  }

  // Remove from sync'd data.
  batch->DeleteData(id);
  store_delegate_->Commit(std::move(batch));

  return true;
}

std::vector<sync_pb::PrinterSpecifics> PrintersSyncBridge::GetAllPrinters()
    const {
  std::vector<sync_pb::PrinterSpecifics> printers;
  for (auto& entry : all_data_) {
    printers.push_back(*entry.second);
  }

  return printers;
}

base::Optional<sync_pb::PrinterSpecifics> PrintersSyncBridge::GetPrinter(
    const std::string& id) const {
  auto iter = all_data_.find(id);
  if (iter == all_data_.end()) {
    return {};
  }

  return {*iter->second};
}

}  // namespace chromeos
