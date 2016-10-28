// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/shared_model_type_processor.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/model_type_processor_proxy.h"
#include "components/sync/model_impl/processor_entity_tracker.h"
#include "components/sync/syncable/syncable_util.h"

namespace syncer {

SharedModelTypeProcessor::SharedModelTypeProcessor(ModelType type,
                                                   ModelTypeSyncBridge* bridge)
    : type_(type),
      is_metadata_loaded_(false),
      is_initial_pending_data_loaded_(false),
      bridge_(bridge),
      error_handler_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(bridge);
}

SharedModelTypeProcessor::~SharedModelTypeProcessor() {}

void SharedModelTypeProcessor::OnSyncStarting(
    std::unique_ptr<DataTypeErrorHandler> error_handler,
    const StartCallback& start_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(start_callback_.is_null());
  DCHECK(!IsConnected());
  DCHECK(error_handler);
  DVLOG(1) << "Sync is starting for " << ModelTypeToString(type_);

  error_handler_ = std::move(error_handler);
  start_callback_ = start_callback;
  ConnectIfReady();
}

void SharedModelTypeProcessor::OnMetadataLoaded(
    SyncError error,
    std::unique_ptr<MetadataBatch> batch) {
  DCHECK(CalledOnValidThread());
  DCHECK(entities_.empty());
  DCHECK(!is_metadata_loaded_);
  DCHECK(!IsConnected());

  is_metadata_loaded_ = true;
  // Flip this flag here to cover all cases where we don't need to load data.
  is_initial_pending_data_loaded_ = true;

  if (error.IsSet()) {
    start_error_ = error;
    ConnectIfReady();
    return;
  }

  if (batch->GetModelTypeState().initial_sync_done()) {
    EntityMetadataMap metadata_map(batch->TakeAllMetadata());
    std::vector<std::string> entities_to_commit;

    for (auto it = metadata_map.begin(); it != metadata_map.end(); it++) {
      std::unique_ptr<ProcessorEntityTracker> entity =
          ProcessorEntityTracker::CreateFromMetadata(it->first, &it->second);
      if (entity->RequiresCommitData()) {
        entities_to_commit.push_back(entity->storage_key());
      }
      storage_key_to_tag_hash_[entity->storage_key()] =
          entity->metadata().client_tag_hash();
      entities_[entity->metadata().client_tag_hash()] = std::move(entity);
    }
    model_type_state_ = batch->GetModelTypeState();
    if (!entities_to_commit.empty()) {
      is_initial_pending_data_loaded_ = false;
      bridge_->GetData(
          entities_to_commit,
          base::Bind(&SharedModelTypeProcessor::OnInitialPendingDataLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    // First time syncing; initialize metadata.
    model_type_state_.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type_));
  }

  ConnectIfReady();
}

void SharedModelTypeProcessor::ConnectIfReady() {
  DCHECK(CalledOnValidThread());
  if (!is_metadata_loaded_ || !is_initial_pending_data_loaded_ ||
      start_callback_.is_null()) {
    return;
  }

  std::unique_ptr<ActivationContext> activation_context;

  if (!start_error_.IsSet()) {
    activation_context = base::MakeUnique<ActivationContext>();
    activation_context->model_type_state = model_type_state_;
    activation_context->type_processor =
        base::MakeUnique<ModelTypeProcessorProxy>(
            weak_ptr_factory_.GetWeakPtr(),
            base::ThreadTaskRunnerHandle::Get());
  }

  start_callback_.Run(start_error_, std::move(activation_context));
  start_callback_.Reset();
}

bool SharedModelTypeProcessor::IsAllowingChanges() const {
  return is_metadata_loaded_;
}

bool SharedModelTypeProcessor::IsConnected() const {
  DCHECK(CalledOnValidThread());
  return !!worker_;
}

void SharedModelTypeProcessor::DisableSync() {
  DCHECK(CalledOnValidThread());
  DCHECK(is_metadata_loaded_);
  std::unique_ptr<MetadataChangeList> change_list =
      bridge_->CreateMetadataChangeList();
  for (auto it = entities_.begin(); it != entities_.end(); ++it) {
    change_list->ClearMetadata(it->second->storage_key());
  }
  change_list->ClearModelTypeState();
  // Nothing to do if this fails, so just ignore the error it might return.
  bridge_->ApplySyncChanges(std::move(change_list), EntityChangeList());
}

bool SharedModelTypeProcessor::IsTrackingMetadata() {
  return model_type_state_.initial_sync_done();
}

SyncError SharedModelTypeProcessor::CreateAndUploadError(
    const tracked_objects::Location& location,
    const std::string& message) {
  if (error_handler_) {
    return error_handler_->CreateAndUploadError(location, message, type_);
  } else {
    return SyncError(location, SyncError::DATATYPE_ERROR, message, type_);
  }
}

void SharedModelTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> worker) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Successfully connected " << ModelTypeToString(type_);

  worker_ = std::move(worker);

  FlushPendingCommitRequests();
}

void SharedModelTypeProcessor::DisconnectSync() {
  DCHECK(CalledOnValidThread());
  DCHECK(IsConnected());

  DVLOG(1) << "Disconnecting sync for " << ModelTypeToString(type_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  worker_.reset();

  for (auto it = entities_.begin(); it != entities_.end(); ++it) {
    it->second->ClearTransientSyncState();
  }
}

void SharedModelTypeProcessor::Put(const std::string& storage_key,
                                   std::unique_ptr<EntityData> data,
                                   MetadataChangeList* metadata_change_list) {
  DCHECK(IsAllowingChanges());
  DCHECK(data.get());
  DCHECK(!data->is_deleted());
  DCHECK(!data->non_unique_name.empty());
  DCHECK_EQ(type_, GetModelTypeFromSpecifics(data->specifics));

  if (!model_type_state_.initial_sync_done()) {
    // Ignore changes before the initial sync is done.
    return;
  }

  // Fill in some data.
  data->client_tag_hash = GetClientTagHash(storage_key, *data);
  if (data->modification_time.is_null()) {
    data->modification_time = base::Time::Now();
  }

  ProcessorEntityTracker* entity = GetEntityForTagHash(data->client_tag_hash);

  if (entity == nullptr) {
    // The bridge is creating a new entity.
    if (data->creation_time.is_null()) {
      data->creation_time = data->modification_time;
    }
    entity = CreateEntity(storage_key, *data);
  } else if (entity->MatchesData(*data)) {
    // Ignore changes that don't actually change anything.
    return;
  }

  entity->MakeLocalChange(std::move(data));
  metadata_change_list->UpdateMetadata(storage_key, entity->metadata());

  FlushPendingCommitRequests();
}

void SharedModelTypeProcessor::Delete(
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {
  DCHECK(IsAllowingChanges());

  if (!model_type_state_.initial_sync_done()) {
    // Ignore changes before the initial sync is done.
    return;
  }

  ProcessorEntityTracker* entity = GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    // That's unusual, but not necessarily a bad thing.
    // Missing is as good as deleted as far as the model is concerned.
    DLOG(WARNING) << "Attempted to delete missing item."
                  << " storage key: " << storage_key;
    return;
  }

  entity->Delete();

  metadata_change_list->UpdateMetadata(storage_key, entity->metadata());
  FlushPendingCommitRequests();
}

void SharedModelTypeProcessor::FlushPendingCommitRequests() {
  CommitRequestDataList commit_requests;

  // Don't bother sending anything if there's no one to send to.
  if (!IsConnected())
    return;

  // Don't send anything if the type is not ready to handle commits.
  if (!model_type_state_.initial_sync_done())
    return;

  // TODO(rlarocque): Do something smarter than iterate here.
  for (auto it = entities_.begin(); it != entities_.end(); ++it) {
    ProcessorEntityTracker* entity = it->second.get();
    if (entity->RequiresCommitRequest() && !entity->RequiresCommitData()) {
      CommitRequestData request;
      entity->InitializeCommitRequestData(&request);
      commit_requests.push_back(request);
    }
  }

  if (!commit_requests.empty())
    worker_->EnqueueForCommit(commit_requests);
}

void SharedModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const CommitResponseDataList& response_list) {
  std::unique_ptr<MetadataChangeList> change_list =
      bridge_->CreateMetadataChangeList();

  model_type_state_ = type_state;
  change_list->UpdateModelTypeState(model_type_state_);

  for (const CommitResponseData& data : response_list) {
    ProcessorEntityTracker* entity = GetEntityForTagHash(data.client_tag_hash);
    if (entity == nullptr) {
      NOTREACHED() << "Received commit response for missing item."
                   << " type: " << type_
                   << " client_tag_hash: " << data.client_tag_hash;
      continue;
    }

    entity->ReceiveCommitResponse(data);

    if (entity->CanClearMetadata()) {
      change_list->ClearMetadata(entity->storage_key());
      entities_.erase(entity->metadata().client_tag_hash());
    } else {
      change_list->UpdateMetadata(entity->storage_key(), entity->metadata());
    }
  }

  SyncError error =
      bridge_->ApplySyncChanges(std::move(change_list), EntityChangeList());
  if (error.IsSet()) {
    error_handler_->OnUnrecoverableError(error);
  }
}

void SharedModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    const UpdateResponseDataList& updates) {
  if (!model_type_state_.initial_sync_done()) {
    OnInitialUpdateReceived(model_type_state, updates);
    return;
  }

  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();
  EntityChangeList entity_changes;

  metadata_changes->UpdateModelTypeState(model_type_state);
  bool got_new_encryption_requirements =
      model_type_state_.encryption_key_name() !=
      model_type_state.encryption_key_name();
  model_type_state_ = model_type_state;

  // If new encryption requirements come from the server, the entities that are
  // in |updates| will be recorded here so they can be ignored during the
  // re-encryption phase at the end.
  std::unordered_set<std::string> already_updated;

  for (const UpdateResponseData& update : updates) {
    ProcessorEntityTracker* entity = ProcessUpdate(update, &entity_changes);

    if (!entity) {
      // The update should be ignored.
      continue;
    }

    if (entity->CanClearMetadata()) {
      metadata_changes->ClearMetadata(entity->storage_key());
      entities_.erase(entity->metadata().client_tag_hash());
    } else {
      metadata_changes->UpdateMetadata(entity->storage_key(),
                                       entity->metadata());
    }

    if (got_new_encryption_requirements) {
      already_updated.insert(entity->storage_key());
    }
  }

  if (got_new_encryption_requirements) {
    RecommitAllForEncryption(already_updated, metadata_changes.get());
  }

  // Inform the bridge of the new or updated data.
  SyncError error =
      bridge_->ApplySyncChanges(std::move(metadata_changes), entity_changes);

  if (error.IsSet()) {
    error_handler_->OnUnrecoverableError(error);
  } else {
    // There may be new reasons to commit by the time this function is done.
    FlushPendingCommitRequests();
  }
}

ProcessorEntityTracker* SharedModelTypeProcessor::ProcessUpdate(
    const UpdateResponseData& update,
    EntityChangeList* entity_changes) {
  const EntityData& data = update.entity.value();
  const std::string& client_tag_hash = data.client_tag_hash;
  ProcessorEntityTracker* entity = GetEntityForTagHash(client_tag_hash);
  if (entity == nullptr) {
    if (data.is_deleted()) {
      DLOG(WARNING) << "Received remote delete for a non-existing item."
                    << " client_tag_hash: " << client_tag_hash;
      return nullptr;
    }

    entity = CreateEntity(data);
    entity_changes->push_back(
        EntityChange::CreateAdd(entity->storage_key(), update.entity));
    entity->RecordAcceptedUpdate(update);
  } else if (entity->UpdateIsReflection(update.response_version)) {
    // Seen this update before; just ignore it.
    return nullptr;
  } else if (entity->IsUnsynced()) {
    ConflictResolution::Type resolution_type =
        ResolveConflict(update, entity, entity_changes);
    UMA_HISTOGRAM_ENUMERATION("Sync.ResolveConflict", resolution_type,
                              ConflictResolution::TYPE_SIZE);
  } else if (data.is_deleted()) {
    // The entity was deleted; inform the bridge. Note that the local data
    // can never be deleted at this point because it would have either been
    // acked (the add case) or pending (the conflict case).
    DCHECK(!entity->metadata().is_deleted());
    entity_changes->push_back(
        EntityChange::CreateDelete(entity->storage_key()));
    entity->RecordAcceptedUpdate(update);
  } else if (!entity->MatchesData(data)) {
    // Specifics have changed, so update the bridge.
    entity_changes->push_back(
        EntityChange::CreateUpdate(entity->storage_key(), update.entity));
    entity->RecordAcceptedUpdate(update);
  } else {
    // No data change; still record that the update was received.
    entity->RecordAcceptedUpdate(update);
  }

  // If the received entity has out of date encryption, we schedule another
  // commit to fix it.
  if (model_type_state_.encryption_key_name() != update.encryption_key_name) {
    DVLOG(2) << ModelTypeToString(type_) << ": Requesting re-encrypt commit "
             << update.encryption_key_name << " -> "
             << model_type_state_.encryption_key_name();

    entity->IncrementSequenceNumber();
    if (entity->RequiresCommitData()) {
      // If there is no pending commit data, then either this update wasn't
      // in conflict or the remote data won; either way the remote data is
      // the right data to re-queue for commit.
      entity->CacheCommitData(update.entity);
    }
  }

  return entity;
}

ConflictResolution::Type SharedModelTypeProcessor::ResolveConflict(
    const UpdateResponseData& update,
    ProcessorEntityTracker* entity,
    EntityChangeList* changes) {
  const EntityData& remote_data = update.entity.value();

  ConflictResolution::Type resolution_type = ConflictResolution::TYPE_SIZE;
  std::unique_ptr<EntityData> new_data;

  // Determine the type of resolution.
  if (entity->MatchesData(remote_data)) {
    // The changes are identical so there isn't a real conflict.
    resolution_type = ConflictResolution::CHANGES_MATCH;
  } else if (entity->RequiresCommitData() ||
             entity->MatchesBaseData(entity->commit_data().value())) {
    // If commit data needs to be loaded at this point, it can only be due to a
    // re-encryption request. If the commit data matches the base data, it also
    // must be a re-encryption request. Either way there's no real local change
    // and the remote data should win.
    resolution_type = ConflictResolution::IGNORE_LOCAL_ENCRYPTION;
  } else if (entity->MatchesBaseData(remote_data)) {
    // The remote data isn't actually changing from the last remote data that
    // was seen, so it must have been a re-encryption and can be ignored.
    resolution_type = ConflictResolution::IGNORE_REMOTE_ENCRYPTION;
  } else {
    // There's a real data conflict here; let the bridge resolve it.
    ConflictResolution resolution =
        bridge_->ResolveConflict(entity->commit_data().value(), remote_data);
    resolution_type = resolution.type();
    new_data = resolution.ExtractData();
  }

  // Apply the resolution.
  switch (resolution_type) {
    case ConflictResolution::CHANGES_MATCH:
      // Record the update and squash the pending commit.
      entity->RecordForcedUpdate(update);
      break;
    case ConflictResolution::USE_LOCAL:
    case ConflictResolution::IGNORE_REMOTE_ENCRYPTION:
      // Record that we received the update from the server but leave the
      // pending commit intact.
      entity->RecordIgnoredUpdate(update);
      break;
    case ConflictResolution::USE_REMOTE:
    case ConflictResolution::IGNORE_LOCAL_ENCRYPTION:
      // Squash the pending commit.
      entity->RecordForcedUpdate(update);
      // Update client data to match server.
      changes->push_back(
          EntityChange::CreateUpdate(entity->storage_key(), update.entity));
      break;
    case ConflictResolution::USE_NEW:
      // Record that we received the update.
      entity->RecordIgnoredUpdate(update);
      // Make a new pending commit to update the server.
      entity->MakeLocalChange(std::move(new_data));
      // Update the client with the new entity.
      changes->push_back(EntityChange::CreateUpdate(entity->storage_key(),
                                                    entity->commit_data()));
      break;
    case ConflictResolution::TYPE_SIZE:
      NOTREACHED();
      break;
  }
  DCHECK(!new_data);

  return resolution_type;
}

void SharedModelTypeProcessor::RecommitAllForEncryption(
    std::unordered_set<std::string> already_updated,
    MetadataChangeList* metadata_changes) {
  ModelTypeSyncBridge::StorageKeyList entities_needing_data;

  for (auto it = entities_.begin(); it != entities_.end(); ++it) {
    ProcessorEntityTracker* entity = it->second.get();
    if (already_updated.find(entity->storage_key()) != already_updated.end()) {
      continue;
    }
    entity->IncrementSequenceNumber();
    if (entity->RequiresCommitData()) {
      entities_needing_data.push_back(entity->storage_key());
    }
    metadata_changes->UpdateMetadata(entity->storage_key(), entity->metadata());
  }

  if (!entities_needing_data.empty()) {
    bridge_->GetData(
        entities_needing_data,
        base::Bind(&SharedModelTypeProcessor::OnDataLoadedForReEncryption,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void SharedModelTypeProcessor::OnInitialUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    const UpdateResponseDataList& updates) {
  DCHECK(entities_.empty());
  // Ensure that initial sync was not already done and that the worker
  // correctly marked initial sync as done for this update.
  DCHECK(!model_type_state_.initial_sync_done());
  DCHECK(model_type_state.initial_sync_done());

  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();
  EntityDataMap data_map;

  model_type_state_ = model_type_state;
  metadata_changes->UpdateModelTypeState(model_type_state_);

  for (const UpdateResponseData& update : updates) {
    ProcessorEntityTracker* entity = CreateEntity(update.entity.value());
    const std::string& storage_key = entity->storage_key();
    entity->RecordAcceptedUpdate(update);
    metadata_changes->UpdateMetadata(storage_key, entity->metadata());
    data_map[storage_key] = update.entity;
  }

  // Let the bridge handle associating and merging the data.
  SyncError error =
      bridge_->MergeSyncData(std::move(metadata_changes), data_map);

  if (error.IsSet()) {
    error_handler_->OnUnrecoverableError(error);
  } else {
    // We may have new reasons to commit by the time this function is done.
    FlushPendingCommitRequests();
  }
}

void SharedModelTypeProcessor::OnInitialPendingDataLoaded(
    SyncError error,
    std::unique_ptr<DataBatch> data_batch) {
  DCHECK(!is_initial_pending_data_loaded_);

  if (error.IsSet()) {
    start_error_ = error;
  } else {
    ConsumeDataBatch(std::move(data_batch));
  }

  is_initial_pending_data_loaded_ = true;
  ConnectIfReady();
}

void SharedModelTypeProcessor::OnDataLoadedForReEncryption(
    SyncError error,
    std::unique_ptr<DataBatch> data_batch) {
  DCHECK(is_initial_pending_data_loaded_);

  if (error.IsSet()) {
    error_handler_->OnUnrecoverableError(error);
    return;
  }

  ConsumeDataBatch(std::move(data_batch));
  FlushPendingCommitRequests();
}

void SharedModelTypeProcessor::ConsumeDataBatch(
    std::unique_ptr<DataBatch> data_batch) {
  while (data_batch->HasNext()) {
    KeyAndData data = data_batch->Next();
    ProcessorEntityTracker* entity = GetEntityForStorageKey(data.first);
    // If the entity wasn't deleted or updated with new commit.
    if (entity != nullptr && entity->RequiresCommitData()) {
      entity->CacheCommitData(data.second.get());
    }
  }
}

std::string SharedModelTypeProcessor::GetHashForTag(const std::string& tag) {
  return syncable::GenerateSyncableHash(type_, tag);
}

std::string SharedModelTypeProcessor::GetClientTagHash(
    const std::string& storage_key,
    const EntityData& data) {
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  return iter == storage_key_to_tag_hash_.end()
             ? GetHashForTag(bridge_->GetClientTag(data))
             : iter->second;
}

ProcessorEntityTracker* SharedModelTypeProcessor::GetEntityForStorageKey(
    const std::string& storage_key) {
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  return iter == storage_key_to_tag_hash_.end()
             ? nullptr
             : GetEntityForTagHash(iter->second);
}

ProcessorEntityTracker* SharedModelTypeProcessor::GetEntityForTagHash(
    const std::string& tag_hash) {
  auto it = entities_.find(tag_hash);
  return it != entities_.end() ? it->second.get() : nullptr;
}

ProcessorEntityTracker* SharedModelTypeProcessor::CreateEntity(
    const std::string& storage_key,
    const EntityData& data) {
  DCHECK(entities_.find(data.client_tag_hash) == entities_.end());
  DCHECK(storage_key_to_tag_hash_.find(storage_key) ==
         storage_key_to_tag_hash_.end());
  std::unique_ptr<ProcessorEntityTracker> entity =
      ProcessorEntityTracker::CreateNew(storage_key, data.client_tag_hash,
                                        data.id, data.creation_time);
  ProcessorEntityTracker* entity_ptr = entity.get();
  entities_[data.client_tag_hash] = std::move(entity);
  storage_key_to_tag_hash_[storage_key] = data.client_tag_hash;
  return entity_ptr;
}

ProcessorEntityTracker* SharedModelTypeProcessor::CreateEntity(
    const EntityData& data) {
  // Verify the tag hash matches, may be relaxed in the future.
  DCHECK_EQ(data.client_tag_hash, GetHashForTag(bridge_->GetClientTag(data)));
  return CreateEntity(bridge_->GetStorageKey(data), data);
}

}  // namespace syncer
