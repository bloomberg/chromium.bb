// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_SYNC_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_error_handler.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/sync_error.h"

namespace syncer {

class DataBatch;
class MetadataChangeList;

// Interface implemented by model types to receive updates from sync via the
// SharedModelTypeProcessor. Provides a way for sync to update the data and
// metadata for entities, as well as the model type state.
class ModelTypeSyncBridge : public base::SupportsWeakPtr<ModelTypeSyncBridge> {
 public:
  typedef base::Callback<void(SyncError, std::unique_ptr<DataBatch>)>
      DataCallback;
  typedef std::vector<std::string> StorageKeyList;
  typedef base::Callback<std::unique_ptr<ModelTypeChangeProcessor>(
      ModelType type,
      ModelTypeSyncBridge* bridge)>
      ChangeProcessorFactory;

  ModelTypeSyncBridge(const ChangeProcessorFactory& change_processor_factory,
                      ModelType type);

  virtual ~ModelTypeSyncBridge();

  // Creates an object used to communicate changes in the sync metadata to the
  // model type store.
  virtual std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() = 0;

  // Perform the initial merge between local and sync data. This should only be
  // called when a data type is first enabled to start syncing, and there is no
  // sync metadata. Best effort should be made to match local and sync data. The
  // keys in the |entity_data_map| will have been created via GetClientTag(...),
  // and if a local and sync data should match/merge but disagree on tags, the
  // bridge should use the sync data's tag. Any local pieces of data that are
  // not present in sync should immediately be Put(...) to the processor before
  // returning. The same MetadataChangeList that was passed into this function
  // can be passed to Put(...) calls. Delete(...) can also be called but should
  // not be needed for most model types. Durable storage writes, if not able to
  // combine all change atomically, should save the metadata after the data
  // changes, so that this merge will be re-driven by sync if is not completely
  // saved during the current run.
  virtual SyncError MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityDataMap entity_data_map) = 0;

  // Apply changes from the sync server locally.
  // Please note that |entity_changes| might have fewer entries than
  // |metadata_change_list| in case when some of the data changes are filtered
  // out, or even be empty in case when a commit confirmation is processed and
  // only the metadata needs to persisted.
  virtual SyncError ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) = 0;

  // Asynchronously retrieve the corresponding sync data for |storage_keys|.
  virtual void GetData(StorageKeyList storage_keys, DataCallback callback) = 0;

  // Asynchronously retrieve all of the local sync data.
  virtual void GetAllData(DataCallback callback) = 0;

  // Get or generate a client tag for |entity_data|. This must be the same tag
  // that was/would have been generated in the SyncableService/Directory world
  // for backward compatibility with pre-USS clients. The only time this
  // theoretically needs to be called is on the creation of local data, however
  // it is also used to verify the hash of remote data. If a data type was never
  // launched pre-USS, then method does not need to be different from
  // GetStorageKey().
  virtual std::string GetClientTag(const EntityData& entity_data) = 0;

  // Get or generate a storage key for |entity_data|. This will only ever be
  // called once when first encountering a remote entity. Local changes will
  // provide their storage keys directly to Put instead of using this method.
  // Theoretically this function doesn't need to be stable across multiple calls
  // on the same or different clients, but to keep things simple, it probably
  // should be.
  virtual std::string GetStorageKey(const EntityData& entity_data) = 0;

  // Resolve a conflict between the client and server versions of data. They are
  // guaranteed not to match (both be deleted or have identical specifics). A
  // default implementation chooses the server data unless it is a deletion.
  virtual ConflictResolution ResolveConflict(
      const EntityData& local_data,
      const EntityData& remote_data) const;

  // Called by the DataTypeController to gather additional information needed
  // before the processor can be connected to a sync worker. Once the
  // metadata has been loaded, the info is collected and given to |callback|.
  void OnSyncStarting(std::unique_ptr<DataTypeErrorHandler> error_handler,
                      const ModelTypeChangeProcessor::StartCallback& callback);

  // Indicates that we no longer want to do any sync-related things for this
  // data type. Severs all ties to the sync thread, deletes all local sync
  // metadata, and then destroys the change processor.
  virtual void DisableSync();

  ModelTypeChangeProcessor* change_processor() const;

 private:
  const ModelType type_;

  const ChangeProcessorFactory change_processor_factory_;

  std::unique_ptr<ModelTypeChangeProcessor> change_processor_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_SYNC_BRIDGE_H_
