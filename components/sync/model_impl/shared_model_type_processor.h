// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_SHARED_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_IMPL_SHARED_MODEL_TYPE_PROCESSOR_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class CommitQueue;
class ProcessorEntityTracker;

// A sync component embedded on the model type's thread that tracks entity
// metadata in the model store and coordinates communication between sync and
// model type threads. See //docs/sync/uss/shared_model_type_processor.md for a
// more thorough description.
class SharedModelTypeProcessor : public ModelTypeProcessor,
                                 public ModelTypeChangeProcessor,
                                 base::NonThreadSafe {
 public:
  SharedModelTypeProcessor(ModelType type,
                           ModelTypeSyncBridge* bridge,
                           const base::RepeatingClosure& dump_stack);
  ~SharedModelTypeProcessor() override;

  // Whether the processor is allowing changes to its model type. If this is
  // false, the bridge should not allow any changes to its data.
  bool IsAllowingChanges() const;

  // Returns true if the handshake with sync thread is complete.
  bool IsConnected() const;

  // ModelTypeChangeProcessor implementation.
  void Put(const std::string& storage_key,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& storage_key,
              MetadataChangeList* metadata_change_list) override;
  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override;
  void OnSyncStarting(const ModelErrorHandler& error_handler,
                      const StartCallback& callback) override;
  void DisableSync() override;
  bool IsTrackingMetadata() override;
  void ReportError(const ModelError& error) override;
  void ReportError(const tracked_objects::Location& location,
                   const std::string& message) override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void OnCommitCompleted(const sync_pb::ModelTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        const UpdateResponseDataList& updates) override;

 private:
  friend class ModelTypeDebugInfo;
  friend class SharedModelTypeProcessorTest;

  // Returns true if the model is ready or encountered an error.
  bool IsModelReadyOrError() const;

  // If preconditions are met, inform sync that we are ready to connect.
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

  // ModelTypeSyncBridge::GetData() callback for initial pending commit data.
  void OnInitialPendingDataLoaded(std::unique_ptr<DataBatch> data_batch);

  // ModelTypeSyncBridge::GetData() callback for re-encryption commit data.
  void OnDataLoadedForReEncryption(std::unique_ptr<DataBatch> data_batch);

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

  /////////////////////
  // Processor state //
  /////////////////////

  // The model type this object syncs.
  const ModelType type_;

  // The model type metadata (progress marker, initial sync done, etc).
  sync_pb::ModelTypeState model_type_state_;

  // ModelTypeSyncBridge linked to this processor. The bridge owns this
  // processor instance so the pointer should never become invalid.
  ModelTypeSyncBridge* const bridge_;

  // Function to capture and upload a stack trace when an error occurs.
  const base::RepeatingClosure dump_stack_;

  /////////////////
  // Model state //
  /////////////////

  // The first model error that occurred, if any. Stored to track model state
  // and so it can be passed to sync if it happened prior to sync being ready.
  base::Optional<ModelError> model_error_;

  // Whether we're waiting for the model to provide metadata.
  bool waiting_for_metadata_ = true;

  // Whether we're waiting for the model to provide initial commit data. Starts
  // as false but will be set to true if we detect it's necessary to load data.
  bool waiting_for_pending_data_ = false;

  ////////////////
  // Sync state //
  ////////////////

  // Stores the start callback in between OnSyncStarting() and ReadyToConnect().
  StartCallback start_callback_;

  // The callback used for informing sync of errors; will be non-null after
  // OnSyncStarting has been called.
  ModelErrorHandler error_handler_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are
  // useful in tests.
  std::unique_ptr<CommitQueue> worker_;

  //////////////////
  // Entity state //
  //////////////////

  // A map of client tag hash to sync entities known to this processor. This
  // should contain entries and metadata for most everything, although the
  // entities may not always contain model type data/specifics.
  std::map<std::string, std::unique_ptr<ProcessorEntityTracker>> entities_;

  // The bridge wants to communicate entirely via storage keys that is free to
  // define and can understand more easily. All of the sync machinery wants to
  // use client tag hash. This mapping allows us to convert from storage key to
  // client tag hash. The other direction can use |entities_|.
  std::map<std::string, std::string> storage_key_to_tag_hash_;

  // WeakPtrFactory for this processor which will be sent to sync thread.
  base::WeakPtrFactory<SharedModelTypeProcessor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedModelTypeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_SHARED_MODEL_TYPE_PROCESSOR_H_
