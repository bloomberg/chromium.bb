// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/client_tag_based_remote_update_handler.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/time.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model_impl/processor_entity.h"
#include "components/sync/model_impl/processor_entity_tracker.h"

namespace syncer {

namespace {

void LogNonReflectionUpdateFreshnessToUma(ModelType type,
                                          base::Time remote_modification_time) {
  const base::TimeDelta latency = base::Time::Now() - remote_modification_time;

  UMA_HISTOGRAM_CUSTOM_TIMES("Sync.NonReflectionUpdateFreshnessPossiblySkewed2",
                             latency,
                             /*min=*/base::TimeDelta::FromMilliseconds(100),
                             /*max=*/base::TimeDelta::FromDays(7),
                             /*bucket_count=*/50);

  base::UmaHistogramCustomTimes(
      std::string("Sync.NonReflectionUpdateFreshnessPossiblySkewed2.") +
          ModelTypeToHistogramSuffix(type),
      latency,
      /*min=*/base::TimeDelta::FromMilliseconds(100),
      /*max=*/base::TimeDelta::FromDays(7),
      /*bucket_count=*/50);
}

}  // namespace

ClientTagBasedRemoteUpdateHandler::ClientTagBasedRemoteUpdateHandler(
    ModelType type,
    ModelTypeSyncBridge* bridge,
    ProcessorEntityTracker* entity_tracker)
    : type_(type),
      bridge_(bridge),
      entity_tracker_(entity_tracker) {
  DCHECK(bridge_);
  DCHECK(entity_tracker_);
}

base::Optional<ModelError>
ClientTagBasedRemoteUpdateHandler::ProcessIncrementalUpdate(
    const sync_pb::ModelTypeState& model_type_state,
    UpdateResponseDataList updates) {
  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();
  EntityChangeList entity_changes;

  metadata_changes->UpdateModelTypeState(model_type_state);
  const bool got_new_encryption_requirements =
      entity_tracker_->model_type_state().encryption_key_name() !=
      model_type_state.encryption_key_name();
  entity_tracker_->set_model_type_state(model_type_state);

  // If new encryption requirements come from the server, the entities that are
  // in |updates| will be recorded here so they can be ignored during the
  // re-encryption phase at the end.
  std::unordered_set<std::string> already_updated;

  for (syncer::UpdateResponseData& update : updates) {
    std::string storage_key_to_clear;
    ProcessorEntity* entity = ProcessUpdate(std::move(update), &entity_changes,
                                            &storage_key_to_clear);

    if (!entity) {
      // The update is either of the following:
      // 1. Tombstone of entity that didn't exist locally.
      // 2. Reflection, thus should be ignored.
      // 3. Update without a client tag hash (including permanent nodes, which
      // have server tags instead).
      continue;
    }

    LogNonReflectionUpdateFreshnessToUma(
        type_,
        /*remote_modification_time=*/
        ProtoTimeToTime(entity->metadata().modification_time()));

    if (entity->storage_key().empty()) {
      // Storage key of this entity is not known yet. Don't update metadata, it
      // will be done from UpdateStorageKey.

      // If this is the result of a conflict resolution (where a remote
      // undeletion was preferred), then need to clear a metadata entry from
      // the database.
      if (!storage_key_to_clear.empty()) {
        metadata_changes->ClearMetadata(storage_key_to_clear);
      }
      continue;
    }

    DCHECK(storage_key_to_clear.empty());

    if (entity->CanClearMetadata()) {
      metadata_changes->ClearMetadata(entity->storage_key());
      entity_tracker_->RemoveEntityForStorageKey(entity->storage_key());
    } else {
      metadata_changes->UpdateMetadata(entity->storage_key(),
                                       entity->metadata());
    }

    if (got_new_encryption_requirements) {
      already_updated.insert(entity->storage_key());
    }
  }

  if (got_new_encryption_requirements) {
    // TODO(pavely): Currently we recommit all entities. We should instead
    // recommit only the ones whose encryption key doesn't match the one in
    // DataTypeState. Work is tracked in http://crbug.com/727874.
    std::vector<const ProcessorEntity*> entities =
        entity_tracker_->IncrementSequenceNumberForAllExcept(already_updated);
    for (const ProcessorEntity* entity : entities) {
      metadata_changes->UpdateMetadata(entity->storage_key(),
                                       entity->metadata());
    }
  }

  // Inform the bridge of the new or updated data.
  return bridge_->ApplySyncChanges(std::move(metadata_changes),
                                   std::move(entity_changes));
}

ProcessorEntity* ClientTagBasedRemoteUpdateHandler::ProcessUpdate(
    UpdateResponseData update,
    EntityChangeList* entity_changes,
    std::string* storage_key_to_clear) {
  const EntityData& data = update.entity;
  const ClientTagHash& client_tag_hash = data.client_tag_hash;

  // Filter out updates without a client tag hash (including permanent nodes,
  // which have server tags instead).
  if (client_tag_hash.value().empty()) {
    return nullptr;
  }

  // Filter out unexpected client tag hashes.
  if (!data.is_deleted() && bridge_->SupportsGetClientTag() &&
      client_tag_hash !=
          ClientTagHash::FromUnhashed(type_, bridge_->GetClientTag(data))) {
    SyncRecordModelTypeUpdateDropReason(
        UpdateDropReason::kInconsistentClientTag, type_);
    DLOG(WARNING) << "Received unexpected client tag hash: " << client_tag_hash
                  << " for " << ModelTypeToString(type_);
    return nullptr;
  }

  ProcessorEntity* entity =
      entity_tracker_->GetEntityForTagHash(client_tag_hash);

  // Handle corner cases first.
  if (entity == nullptr && data.is_deleted()) {
    // Local entity doesn't exist and update is tombstone.
    SyncRecordModelTypeUpdateDropReason(
        UpdateDropReason::kTombstoneForNonexistentInIncrementalUpdate, type_);
    DLOG(WARNING) << "Received remote delete for a non-existing item."
                  << " client_tag_hash: " << client_tag_hash << " for "
                  << ModelTypeToString(type_);
    return nullptr;
  }

  if (entity) {
    entity->RecordEntityUpdateLatency(update.response_version, type_);
  }

  if (entity && entity->UpdateIsReflection(update.response_version)) {
    // Seen this update before; just ignore it.
    return nullptr;
  }

  // Cache update encryption_key_name and is_deleted in case |update| will be
  // moved away into ResolveConflict().
  const std::string update_encryption_key_name = update.encryption_key_name;
  const bool update_is_tombstone = data.is_deleted();
  if (entity && entity->IsUnsynced()) {
    ResolveConflict(std::move(update), entity, entity_changes,
                    storage_key_to_clear);
  } else {
    // Handle simple create/delete/update.
    base::Optional<EntityChange::ChangeType> change_type;

    if (entity == nullptr) {
      entity = CreateEntity(data);
      change_type = EntityChange::ACTION_ADD;
    } else if (data.is_deleted()) {
      DCHECK(!entity->metadata().is_deleted());
      change_type = EntityChange::ACTION_DELETE;
    } else if (!entity->MatchesData(data)) {
      change_type = EntityChange::ACTION_UPDATE;
    }
    entity->RecordAcceptedUpdate(update);
    // Inform the bridge about the changes if needed.
    if (change_type) {
      switch (change_type.value()) {
        case EntityChange::ACTION_ADD:
          entity_changes->push_back(EntityChange::CreateAdd(
              entity->storage_key(), std::move(update.entity)));
          break;
        case EntityChange::ACTION_DELETE:
          // The entity was deleted; inform the bridge. Note that the local data
          // can never be deleted at this point because it would have either
          // been acked (the add case) or pending (the conflict case).
          entity_changes->push_back(
              EntityChange::CreateDelete(entity->storage_key()));
          break;
        case EntityChange::ACTION_UPDATE:
          // Specifics have changed, so update the bridge.
          entity_changes->push_back(EntityChange::CreateUpdate(
              entity->storage_key(), std::move(update.entity)));
          break;
      }
    }
  }

  // If the received entity has out of date encryption, we schedule another
  // commit to fix it. Tombstones aren't encrypted and hence shouldn't be
  // checked.
  if (!update_is_tombstone &&
      entity_tracker_->model_type_state().encryption_key_name() !=
          update_encryption_key_name) {
    DVLOG(2) << ModelTypeToString(type_) << ": Requesting re-encrypt commit "
             << update_encryption_key_name << " -> "
             << entity_tracker_->model_type_state().encryption_key_name();

    entity->IncrementSequenceNumber(base::Time::Now());
  }
  return entity;
}

void ClientTagBasedRemoteUpdateHandler::ResolveConflict(
    UpdateResponseData update,
    ProcessorEntity* entity,
    EntityChangeList* changes,
    std::string* storage_key_to_clear) {
  const EntityData& remote_data = update.entity;

  ConflictResolution resolution_type = ConflictResolution::kChangesMatch;

  // Determine the type of resolution.
  if (entity->MatchesData(remote_data)) {
    // The changes are identical so there isn't a real conflict.
    resolution_type = ConflictResolution::kChangesMatch;
  } else if (entity->metadata().is_deleted()) {
    // Local tombstone vs remote update (non-deletion). Should be undeleted.
    resolution_type = ConflictResolution::kUseRemote;
  } else if (entity->MatchesOwnBaseData()) {
    // If there is no real local change, then the entity must be unsynced due to
    // a pending local re-encryption request. In this case, the remote data
    // should win.
    resolution_type = ConflictResolution::kIgnoreLocalEncryption;
  } else if (entity->MatchesBaseData(remote_data)) {
    // The remote data isn't actually changing from the last remote data that
    // was seen, so it must have been a re-encryption and can be ignored.
    resolution_type = ConflictResolution::kIgnoreRemoteEncryption;
  } else {
    // There's a real data conflict here; let the bridge resolve it.
    resolution_type =
        bridge_->ResolveConflict(entity->storage_key(), remote_data);
  }

  // Apply the resolution.
  switch (resolution_type) {
    case ConflictResolution::kChangesMatch:
      // Record the update and squash the pending commit.
      entity->RecordForcedUpdate(update);
      break;
    case ConflictResolution::kUseLocal:
    case ConflictResolution::kIgnoreRemoteEncryption:
      // Record that we received the update from the server but leave the
      // pending commit intact.
      entity->RecordIgnoredUpdate(update);
      break;
    case ConflictResolution::kUseRemote:
    case ConflictResolution::kIgnoreLocalEncryption:
      // Update client data to match server.
      if (update.entity.is_deleted()) {
        DCHECK(!entity->metadata().is_deleted());
        // Squash the pending commit.
        entity->RecordForcedUpdate(update);
        changes->push_back(EntityChange::CreateDelete(entity->storage_key()));
      } else if (!entity->metadata().is_deleted()) {
        // Squash the pending commit.
        entity->RecordForcedUpdate(update);
        changes->push_back(EntityChange::CreateUpdate(
            entity->storage_key(), std::move(update.entity)));
      } else {
        // Remote undeletion. This could imply a new storage key for some
        // bridges, so we may need to wait until UpdateStorageKey() is called.
        if (!bridge_->SupportsGetStorageKey()) {
          *storage_key_to_clear = entity->storage_key();
          entity_tracker_->ClearStorageKey(entity->storage_key());
          DCHECK(entity->storage_key().empty());
        }
        // Squash the pending commit.
        entity->RecordForcedUpdate(update);
        changes->push_back(EntityChange::CreateAdd(entity->storage_key(),
                                                   std::move(update.entity)));
      }
      break;
  }
}

ProcessorEntity* ClientTagBasedRemoteUpdateHandler::CreateEntity(
    const std::string& storage_key,
    const EntityData& data) {
  DCHECK(!data.client_tag_hash.value().empty());
  DCHECK(!bridge_->SupportsGetStorageKey() || !storage_key.empty());
  return entity_tracker_->Add(storage_key, data);
}

ProcessorEntity* ClientTagBasedRemoteUpdateHandler::CreateEntity(
    const EntityData& data) {
  if (bridge_->SupportsGetClientTag()) {
    DCHECK_EQ(data.client_tag_hash,
              ClientTagHash::FromUnhashed(type_, bridge_->GetClientTag(data)));
  }
  std::string storage_key;
  if (bridge_->SupportsGetStorageKey())
    storage_key = bridge_->GetStorageKey(data);
  return CreateEntity(storage_key, data);
}

}  // namespace syncer
