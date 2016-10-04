// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_SHARED_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_CORE_SHARED_MODEL_TYPE_PROCESSOR_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/api/data_batch.h"
#include "components/sync/api/data_type_error_handler.h"
#include "components/sync/api/metadata_batch.h"
#include "components/sync/api/metadata_change_list.h"
#include "components/sync/api/model_type_change_processor.h"
#include "components/sync/api/model_type_service.h"
#include "components/sync/api/sync_error.h"
#include "components/sync/base/model_type.h"
#include "components/sync/core/model_type_processor.h"
#include "components/sync/core/non_blocking_sync_common.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {
struct ActivationContext;
class CommitQueue;
class ProcessorEntityTracker;

// A sync component embedded on the synced type's thread that helps to handle
// communication between sync and model type threads.
class SharedModelTypeProcessor : public ModelTypeProcessor,
                                 public ModelTypeChangeProcessor,
                                 base::NonThreadSafe {
 public:
  SharedModelTypeProcessor(ModelType type, ModelTypeService* service);
  ~SharedModelTypeProcessor() override;

  // An easily bound function that constructs a SharedModelTypeProcessor.
  static std::unique_ptr<ModelTypeChangeProcessor> CreateAsChangeProcessor(
      ModelType type,
      ModelTypeService* service);

  // Whether the processor is allowing changes to its model type. If this is
  // false, the service should not allow any changes to its data.
  bool IsAllowingChanges() const;

  // Returns true if the handshake with sync thread is complete.
  bool IsConnected() const;

  // Returns a ListValue representing all nodes for data type |type| through
  // |callback| on this thread.
  // Used for populating nodes in Sync Node Browser of chrome://sync-internals.
  // TODO(gangwu): GetAllNodes could be in a helper class.
  void GetAllNodes(
      const scoped_refptr<base::TaskRunner>& task_runner,
      const base::Callback<void(const ModelType type,
                                std::unique_ptr<base::ListValue>)>& callback);

  // ModelTypeChangeProcessor implementation.
  void Put(const std::string& storage_key,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& storage_key,
              MetadataChangeList* metadata_change_list) override;
  void OnMetadataLoaded(SyncError error,
                        std::unique_ptr<MetadataBatch> batch) override;
  void OnSyncStarting(std::unique_ptr<DataTypeErrorHandler> error_handler,
                      const StartCallback& callback) override;
  void DisableSync() override;
  SyncError CreateAndUploadError(const tracked_objects::Location& location,
                                 const std::string& message) override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void OnCommitCompleted(const sync_pb::ModelTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        const UpdateResponseDataList& updates) override;

 private:
  friend class SharedModelTypeProcessorTest;

  using EntityMap =
      std::map<std::string, std::unique_ptr<ProcessorEntityTracker>>;
  using UpdateMap = std::map<std::string, std::unique_ptr<UpdateResponseData>>;

  // Check conditions, and if met inform sync that we are ready to connect.
  void ConnectIfReady();

  // Helper function to process the update for a single entity. If a local data
  // change is required, it will be added to |entity_changes|. The return value
  // is the tracker for this entity, or nullptr if the update should be ignored.
  ProcessorEntityTracker* ProcessUpdate(const UpdateResponseData& update,
                                        EntityChangeList* entity_changes);

  // Resolve a conflict between |update| and the pending commit in |entity|.
  ConflictResolution::Type ResolveConflict(const UpdateResponseData& update,
                                           ProcessorEntityTracker* entity,
                                           EntityChangeList* changes);

  // Recommit all entities for encryption except those in |already_updated|.
  void RecommitAllForEncryption(std::unordered_set<std::string> already_updated,
                                MetadataChangeList* metadata_changes);

  // Handle the first update received from the server after being enabled.
  void OnInitialUpdateReceived(const sync_pb::ModelTypeState& type_state,
                               const UpdateResponseDataList& updates);

  // ModelTypeService::GetData() callback for initial pending commit data.
  void OnInitialPendingDataLoaded(SyncError error,
                                  std::unique_ptr<DataBatch> data_batch);

  // ModelTypeService::GetData() callback for re-encryption commit data.
  void OnDataLoadedForReEncryption(SyncError error,
                                   std::unique_ptr<DataBatch> data_batch);

  // Caches EntityData from the |data_batch| in the entity trackers.
  void ConsumeDataBatch(std::unique_ptr<DataBatch> data_batch);

  // Sends all commit requests that are due to be sent to the sync thread.
  void FlushPendingCommitRequests();

  // Computes the client tag hash for the given client |tag|.
  std::string GetHashForTag(const std::string& tag);

  // Looks up the client tag hash for the given |storage_key|, and regenerates
  // with |data| if the lookup finds nothing. Does not update the storage key to
  // client tag hash mapping.
  std::string GetClientTagHash(const std::string& storage_key,
                               const EntityData& data);

  // Gets the entity for the given storage key, or null if there isn't one.
  ProcessorEntityTracker* GetEntityForStorageKey(
      const std::string& storage_key);

  // Gets the entity for the given tag hash, or null if there isn't one.
  ProcessorEntityTracker* GetEntityForTagHash(const std::string& tag_hash);

  // Create an entity in the entity map for |storage_key| and return a pointer
  // to it.
  // Requires that no entity for |storage_key| already exists in the map.
  ProcessorEntityTracker* CreateEntity(const std::string& storage_key,
                                       const EntityData& data);

  // Version of the above that generates a tag for |data|.
  ProcessorEntityTracker* CreateEntity(const EntityData& data);

  // This is callback function for ModelTypeService::GetAllData. This function
  // will merge real data |batch| with metadata, then pass to |callback|.
  void MergeDataWithMetadata(
      const scoped_refptr<base::TaskRunner>& task_runner,
      const base::Callback<void(const ModelType,
                                std::unique_ptr<base::ListValue>)>& callback,
      SyncError error,
      std::unique_ptr<DataBatch> batch);

  const ModelType type_;
  sync_pb::ModelTypeState model_type_state_;

  // Stores the start callback in between OnSyncStarting() and ReadyToConnect().
  StartCallback start_callback_;

  // A cache for any error that may occur during startup and should be passed
  // into the |start_callback_|.
  SyncError start_error_;

  // Indicates whether the metadata has finished loading.
  bool is_metadata_loaded_;

  // Indicates whether data for any initial pending commits has been loaded.
  bool is_initial_pending_data_loaded_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are
  // useful in tests.
  std::unique_ptr<CommitQueue> worker_;

  // A map of client tag hash to sync entities known to this processor. This
  // should contain entries and metadata for most everything, although the
  // entities may not always contain model type data/specifics.
  EntityMap entities_;

  // The service wants to communicate entirly via storage keys that is free to
  // define and can understand more easily. All of the sync machinery wants to
  // use client tag hash. This mapping allows us to convert from storage key to
  // client tag hash. The other direction can use |entities_|.
  std::map<std::string, std::string> storage_key_to_tag_hash_;

  // ModelTypeService linked to this processor.
  // The service owns this processor instance so the pointer should never
  // become invalid.
  ModelTypeService* const service_;

  // The object used for informing sync of errors; will be non-null after
  // OnSyncStarting has been called. This pointer is not owned.
  std::unique_ptr<DataTypeErrorHandler> error_handler_;

  // WeakPtrFactory for this processor which will be sent to sync thread.
  base::WeakPtrFactory<SharedModelTypeProcessor> weak_ptr_factory_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_SHARED_MODEL_TYPE_PROCESSOR_H_
