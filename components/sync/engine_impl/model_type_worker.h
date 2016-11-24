// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_MODEL_TYPE_WORKER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_MODEL_TYPE_WORKER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/sync/base/cryptographer.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine_impl/commit_contributor.h"
#include "components/sync/engine_impl/cycle/data_type_debug_info_emitter.h"
#include "components/sync/engine_impl/nudge_handler.h"
#include "components/sync/engine_impl/update_handler.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class ModelTypeProcessor;
class WorkerEntityTracker;

// A smart cache for sync types that use message passing (rather than
// transactions and the syncable::Directory) to communicate with the sync
// thread.
//
// When the non-blocking sync type wants to talk with the sync server, it will
// send a message from its thread to this object on the sync thread. This
// object ensures the appropriate sync server communication gets scheduled and
// executed. The response, if any, will be returned to the non-blocking sync
// type's thread eventually.
//
// This object also has a role to play in communications in the opposite
// direction. Sometimes the sync thread will receive changes from the sync
// server and deliver them here. This object will post this information back to
// the appropriate component on the model type's thread.
//
// This object does more than just pass along messages. It understands the sync
// protocol, and it can make decisions when it sees conflicting messages. For
// example, if the sync server sends down an update for a sync entity that is
// currently pending for commit, this object will detect this condition and
// cancel the pending commit.
class ModelTypeWorker : public UpdateHandler,
                        public CommitContributor,
                        public CommitQueue {
 public:
  ModelTypeWorker(ModelType type,
                  const sync_pb::ModelTypeState& initial_state,
                  bool trigger_initial_sync,
                  std::unique_ptr<Cryptographer> cryptographer,
                  NudgeHandler* nudge_handler,
                  std::unique_ptr<ModelTypeProcessor> model_type_processor,
                  DataTypeDebugInfoEmitter* debug_info_emitter);
  ~ModelTypeWorker() override;

  ModelType GetModelType() const;

  void UpdateCryptographer(std::unique_ptr<Cryptographer> cryptographer);

  // UpdateHandler implementation.
  bool IsInitialSyncEnded() const override;
  void GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const override;
  void GetDataTypeContext(sync_pb::DataTypeContext* context) const override;
  SyncerError ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      StatusController* status) override;
  void ApplyUpdates(StatusController* status) override;
  void PassiveApplyUpdates(StatusController* status) override;

  // CommitQueue implementation.
  void EnqueueForCommit(const CommitRequestDataList& request_list) override;

  // CommitContributor implementation.
  std::unique_ptr<CommitContribution> GetContribution(
      size_t max_entries) override;

  // An alternative way to drive sending data to the processor, that should be
  // called when a new encryption mechanism is ready.
  void EncryptionAcceptedApplyUpdates();

  // Callback for when our contribution gets a response.
  void OnCommitResponse(CommitResponseDataList* response_list);

  // If migration the directory encounters an error partway through, we need to
  // clear the update data that has been added so far.
  void AbortMigration();

  base::WeakPtr<ModelTypeWorker> AsWeakPtr();

 private:
  using EntityMap = std::map<std::string, std::unique_ptr<WorkerEntityTracker>>;

  // Helper function to actually send |pending_updates_| to the processor.
  void ApplyPendingUpdates();

  // Returns true if this type has successfully fetched all available updates
  // from the server at least once. Our state may or may not be stale, but at
  // least we know that it was valid at some point in the past.
  bool IsTypeInitialized() const;

  // Returns true if this type is prepared to commit items. Currently, this
  // depends on having downloaded the initial data and having the encryption
  // settings in a good state.
  bool CanCommitItems() const;

  // Returns true if this type should stop communicating because of outstanding
  // encryption issues and must wait for keys to be updated.
  bool BlockForEncryption() const;

  // Takes |commit_entity| populated from fields of WorkerEntityTracker and
  // adjusts some fields before committing to server. Adjustments include
  // generating client-assigned ID, encrypting data, etc.
  void AdjustCommitProto(sync_pb::SyncEntity* commit_entity);

  // Attempts to decrypt encrypted updates stored in the EntityMap. If
  // successful, will remove the update from the its tracker and forward
  // it to the processor for application. Will forward any new encryption
  // keys to the processor to trigger re-encryption if necessary.
  void OnCryptographerUpdated();

  // Updates the encryption key name stored in |model_type_state_| if it differs
  // from the default encryption key name in |cryptographer_|. Returns whether
  // an update occured.
  bool UpdateEncryptionKeyName();

  // Iterates through all elements in |entities_| and tries to decrypt anything
  // that has encrypted data. Also updates |has_encrypted_updates_| to reflect
  // whether anything in |entities_| was not decryptable by |cryptographer_|.
  // Should only be called during a GetUpdates cycle.
  void DecryptedStoredEntities();

  // Attempts to decrypt the given specifics and return them in the |out|
  // parameter. Assumes cryptographer_->CanDecrypt(specifics) returned true.
  //
  // Returns false if the decryption failed. There are no guarantees about the
  // contents of |out| when that happens.
  //
  // In theory, this should never fail. Only corrupt or invalid entries could
  // cause this to fail, and no clients are known to create such entries. The
  // failure case is an attempt to be defensive against bad input.
  bool DecryptSpecifics(const sync_pb::EntitySpecifics& in,
                        sync_pb::EntitySpecifics* out);

  // Returns the entity tracker for the given |tag_hash|, or nullptr.
  WorkerEntityTracker* GetEntityTracker(const std::string& tag_hash);

  // Creates an entity tracker in the map using the given |data| and returns a
  // pointer to it. Requires that one doesn't exist for data.client_tag_hash.
  WorkerEntityTracker* CreateEntityTracker(const EntityData& data);

  // Gets the entity tracker for |data| or creates one if it doesn't exist.
  WorkerEntityTracker* GetOrCreateEntityTracker(const EntityData& data);

  ModelType type_;
  DataTypeDebugInfoEmitter* debug_info_emitter_;

  // State that applies to the entire model type.
  sync_pb::ModelTypeState model_type_state_;

  // Pointer to the ModelTypeProcessor associated with this worker. Never null.
  std::unique_ptr<ModelTypeProcessor> model_type_processor_;

  // A private copy of the most recent cryptographer known to sync.
  // Initialized at construction time and updated with UpdateCryptographer().
  // null if encryption is not enabled for this type.
  std::unique_ptr<Cryptographer> cryptographer_;

  // Interface used to access and send nudges to the sync scheduler. Not owned.
  NudgeHandler* nudge_handler_;

  // A map of per-entity information, keyed by client_tag_hash.
  //
  // When commits are pending, their information is stored here. This
  // information is dropped from memory when the commit succeeds or gets
  // cancelled.
  //
  // This also stores some information related to received server state in
  // order to implement reflection blocking and conflict detection. This
  // information is kept in memory indefinitely.
  EntityMap entities_;

  // Accumulates all the updates from a single GetUpdates cycle in memory so
  // they can all be sent to the processor at once.
  UpdateResponseDataList pending_updates_;

  // Whether there are outstanding encrypted updates in |entities_|.
  bool has_encrypted_updates_ = false;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<ModelTypeWorker> weak_ptr_factory_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_MODEL_TYPE_WORKER_H_
