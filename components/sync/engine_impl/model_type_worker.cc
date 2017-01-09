// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_worker.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine_impl/commit_contribution.h"
#include "components/sync/engine_impl/non_blocking_type_commit_contribution.h"
#include "components/sync/engine_impl/worker_entity_tracker.h"
#include "components/sync/syncable/syncable_util.h"

namespace syncer {

ModelTypeWorker::ModelTypeWorker(
    ModelType type,
    const sync_pb::ModelTypeState& initial_state,
    bool trigger_initial_sync,
    std::unique_ptr<Cryptographer> cryptographer,
    NudgeHandler* nudge_handler,
    std::unique_ptr<ModelTypeProcessor> model_type_processor,
    DataTypeDebugInfoEmitter* debug_info_emitter)
    : type_(type),
      debug_info_emitter_(debug_info_emitter),
      model_type_state_(initial_state),
      model_type_processor_(std::move(model_type_processor)),
      cryptographer_(std::move(cryptographer)),
      nudge_handler_(nudge_handler),
      weak_ptr_factory_(this) {
  DCHECK(model_type_processor_);

  // Request an initial sync if it hasn't been completed yet.
  if (trigger_initial_sync) {
    nudge_handler_->NudgeForInitialDownload(type_);
  }

  // This case handles the scenario where the processor has a serialized model
  // type state that has already done its initial sync, and is going to be
  // tracking metadata changes, however it does not have the most recent
  // encryption key name. The cryptographer was updated while the worker was not
  // around, and we're not going to recieve the normal UpdateCryptographer() or
  // EncryptionAcceptedApplyUpdates() calls to drive this process.
  //
  // If |cryptographer_->is_ready()| is false, all the rest of this logic can be
  // safely skipped, since |UpdateCryptographer(...)| must be called first and
  // things should be driven normally after that.
  //
  // If |model_type_state_.initial_sync_done()| is false, |model_type_state_|
  // may still need to be updated, since UpdateCryptographer() is never going to
  // happen, but we can assume PassiveApplyUpdates(...) will push the state to
  // the processor, and we should not push it now. In fact, doing so now would
  // violate the processor's assumption that the first OnUpdateReceived is will
  // be changing initial sync done to true.
  if (cryptographer_ && cryptographer_->is_ready() &&
      UpdateEncryptionKeyName() && model_type_state_.initial_sync_done()) {
    ApplyPendingUpdates();
  }
}

ModelTypeWorker::~ModelTypeWorker() {
  model_type_processor_->DisconnectSync();
}

ModelType ModelTypeWorker::GetModelType() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return type_;
}

void ModelTypeWorker::UpdateCryptographer(
    std::unique_ptr<Cryptographer> cryptographer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(cryptographer);
  cryptographer_ = std::move(cryptographer);
  OnCryptographerUpdated();
}

// UpdateHandler implementation.
bool ModelTypeWorker::IsInitialSyncEnded() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return model_type_state_.initial_sync_done();
}

void ModelTypeWorker::GetDownloadProgress(
    sync_pb::DataTypeProgressMarker* progress_marker) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  progress_marker->CopyFrom(model_type_state_.progress_marker());
}

void ModelTypeWorker::GetDataTypeContext(
    sync_pb::DataTypeContext* context) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  context->CopyFrom(model_type_state_.type_context());
}

SyncerError ModelTypeWorker::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    StatusController* status) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(rlarocque): Handle data type context conflicts.
  *model_type_state_.mutable_type_context() = mutated_context;
  *model_type_state_.mutable_progress_marker() = progress_marker;

  UpdateCounters* counters = debug_info_emitter_->GetMutableUpdateCounters();
  counters->num_updates_received += applicable_updates.size();

  for (const sync_pb::SyncEntity* update_entity : applicable_updates) {
    // Skip updates for permanent folders.
    // TODO(crbug.com/516866): might need to handle this for hierarchical types.
    if (!update_entity->server_defined_unique_tag().empty())
      continue;

    // Normal updates are handled here.
    const std::string& client_tag_hash =
        update_entity->client_defined_unique_tag();

    // TODO(crbug.com/516866): this wouldn't be true for bookmarks.
    DCHECK(!client_tag_hash.empty());

    // Prepare the message for the model thread.
    EntityData data;
    data.id = update_entity->id_string();
    data.client_tag_hash = client_tag_hash;
    data.creation_time = ProtoTimeToTime(update_entity->ctime());
    data.modification_time = ProtoTimeToTime(update_entity->mtime());
    data.non_unique_name = update_entity->name();

    UpdateResponseData response_data;
    response_data.response_version = update_entity->version();

    WorkerEntityTracker* entity = GetOrCreateEntityTracker(data);

    if (!entity->UpdateContainsNewVersion(response_data)) {
      status->increment_num_reflected_updates_downloaded_by(1);
      ++counters->num_reflected_updates_received;
    }
    if (update_entity->deleted()) {
      status->increment_num_tombstone_updates_downloaded_by(1);
      ++counters->num_tombstone_updates_received;
    }

    // Deleted entities must use the default instance of EntitySpecifics in
    // order for EntityData to correctly reflect that they are deleted.
    const sync_pb::EntitySpecifics& specifics =
        update_entity->deleted() ? sync_pb::EntitySpecifics::default_instance()
                                 : update_entity->specifics();

    // Check if specifics are encrypted and try to decrypt if so.
    if (!specifics.has_encrypted()) {
      // No encryption.
      data.specifics = specifics;
      response_data.entity = data.PassToPtr();
      entity->ReceiveUpdate(response_data);
      pending_updates_.push_back(response_data);
    } else if (cryptographer_ &&
               cryptographer_->CanDecrypt(specifics.encrypted())) {
      // Encrypted and we know the key.
      if (DecryptSpecifics(specifics, &data.specifics)) {
        response_data.entity = data.PassToPtr();
        response_data.encryption_key_name = specifics.encrypted().key_name();
        entity->ReceiveUpdate(response_data);
        pending_updates_.push_back(response_data);
      }
    } else {
      // Can't decrypt right now. Ask the entity tracker to handle it.
      data.specifics = specifics;
      response_data.entity = data.PassToPtr();
      entity->ReceiveEncryptedUpdate(response_data);
      has_encrypted_updates_ = true;
    }
  }

  debug_info_emitter_->EmitUpdateCountersUpdate();
  return SYNCER_OK;
}

void ModelTypeWorker::ApplyUpdates(StatusController* status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This should only ever be called after one PassiveApplyUpdates.
  DCHECK(model_type_state_.initial_sync_done());
  // Download cycle is done, pass all updates to the processor.
  ApplyPendingUpdates();
}

void ModelTypeWorker::PassiveApplyUpdates(StatusController* status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This should only be called at the end of the very first download cycle.
  DCHECK(!model_type_state_.initial_sync_done());
  // Indicate to the processor that the initial download is done. The initial
  // sync technically isn't done yet but by the time this value is persisted to
  // disk on the model thread it will be.
  model_type_state_.set_initial_sync_done(true);
  ApplyPendingUpdates();
}

void ModelTypeWorker::EncryptionAcceptedApplyUpdates() {
  DCHECK(cryptographer_);
  DCHECK(cryptographer_->is_ready());
  // Reuse ApplyUpdates(...) to get its DCHECKs as well.
  ApplyUpdates(nullptr);
}

void ModelTypeWorker::ApplyPendingUpdates() {
  if (!BlockForEncryption()) {
    DVLOG(1) << ModelTypeToString(type_) << ": "
             << base::StringPrintf("Delivering %" PRIuS " applicable updates.",
                                   pending_updates_.size());

    // If there are still encrypted updates left at this point, they're about to
    // to be potentially lost if the progress marker is saved to disk. Typically
    // the nigori update should arrive simultaneously with the first of the
    // encrypted data. It is possible that non-immediately consistent updates do
    // not follow this pattern.
    UMA_HISTOGRAM_BOOLEAN("Sync.WorkerApplyHasEncryptedUpdates",
                          has_encrypted_updates_);
    DCHECK(!has_encrypted_updates_);

    model_type_processor_->OnUpdateReceived(model_type_state_,
                                            pending_updates_);

    UpdateCounters* counters = debug_info_emitter_->GetMutableUpdateCounters();
    counters->num_updates_applied += pending_updates_.size();
    debug_info_emitter_->EmitUpdateCountersUpdate();
    debug_info_emitter_->EmitStatusCountersUpdate();

    pending_updates_.clear();
  }
}

void ModelTypeWorker::EnqueueForCommit(const CommitRequestDataList& list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsTypeInitialized())
      << "Asked to commit items before type was initialized. "
      << "ModelType is: " << ModelTypeToString(type_);

  for (const CommitRequestData& commit : list) {
    const EntityData& data = commit.entity.value();
    if (!data.is_deleted()) {
      DCHECK_EQ(type_, GetModelTypeFromSpecifics(data.specifics));
    }
    GetOrCreateEntityTracker(data)->RequestCommit(commit);
  }

  if (CanCommitItems())
    nudge_handler_->NudgeForCommit(type_);
}

// CommitContributor implementation.
std::unique_ptr<CommitContribution> ModelTypeWorker::GetContribution(
    size_t max_entries) {
  DCHECK(thread_checker_.CalledOnValidThread());

  size_t space_remaining = max_entries;
  google::protobuf::RepeatedPtrField<sync_pb::SyncEntity> commit_entities;

  if (!CanCommitItems())
    return std::unique_ptr<CommitContribution>();

  // TODO(rlarocque): Avoid iterating here.
  for (EntityMap::const_iterator it = entities_.begin();
       it != entities_.end() && space_remaining > 0; ++it) {
    WorkerEntityTracker* entity = it->second.get();
    if (entity->HasPendingCommit()) {
      sync_pb::SyncEntity* commit_entity = commit_entities.Add();
      entity->PopulateCommitProto(commit_entity);
      AdjustCommitProto(commit_entity);
      space_remaining--;
    }
  }

  if (commit_entities.size() == 0)
    return std::unique_ptr<CommitContribution>();

  return base::MakeUnique<NonBlockingTypeCommitContribution>(
      model_type_state_.type_context(), commit_entities, this,
      debug_info_emitter_);
}

void ModelTypeWorker::OnCommitResponse(CommitResponseDataList* response_list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (CommitResponseData& response : *response_list) {
    WorkerEntityTracker* entity = GetEntityTracker(response.client_tag_hash);

    // There's no way we could have committed an entry we know nothing about.
    if (entity == nullptr) {
      NOTREACHED() << "Received commit response for item unknown to us."
                   << " Model type: " << ModelTypeToString(type_)
                   << " ID: " << response.id;
      continue;
    }

    // Remember if entity was deleted. After ReceiveCommitResponse this flag
    // will not be available.
    bool is_deletion = entity->PendingCommitIsDeletion();

    entity->ReceiveCommitResponse(&response);

    if (is_deletion) {
      entities_.erase(response.client_tag_hash);
    }
  }

  // Send the responses back to the model thread. It needs to know which
  // items have been successfully committed so it can save that information in
  // permanent storage.
  model_type_processor_->OnCommitCompleted(model_type_state_, *response_list);
}

void ModelTypeWorker::AbortMigration() {
  DCHECK(!model_type_state_.initial_sync_done());
  model_type_state_ = sync_pb::ModelTypeState();
  entities_.clear();
  pending_updates_.clear();
  has_encrypted_updates_ = false;
  nudge_handler_->NudgeForInitialDownload(type_);
}

base::WeakPtr<ModelTypeWorker> ModelTypeWorker::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool ModelTypeWorker::IsTypeInitialized() const {
  return model_type_state_.initial_sync_done() &&
         !model_type_state_.progress_marker().token().empty();
}

bool ModelTypeWorker::CanCommitItems() const {
  // We can only commit if we've received the initial update response and aren't
  // blocked by missing encryption keys.
  return IsTypeInitialized() && !BlockForEncryption();
}

bool ModelTypeWorker::BlockForEncryption() const {
  // Should be using encryption, but we do not have the keys.
  return cryptographer_ && !cryptographer_->is_ready();
}

void ModelTypeWorker::AdjustCommitProto(sync_pb::SyncEntity* sync_entity) {
  DCHECK(CanCommitItems());

  // Initial commits need our help to generate a client ID.
  if (sync_entity->version() == kUncommittedVersion) {
    DCHECK(sync_entity->id_string().empty());
    // TODO(crbug.com/516866): This is incorrect for bookmarks for two reasons:
    // 1) Won't be able to match previously committed bookmarks to the ones
    //    with server ID.
    // 2) Recommitting an item in a case of failing to receive commit response
    //    would result in generating a different client ID, which in turn
    //    would result in a duplication.
    // We should generate client ID on the frontend side instead.
    sync_entity->set_id_string(base::GenerateGUID());
    sync_entity->set_version(0);
  } else {
    DCHECK(!sync_entity->id_string().empty());
  }

  // Encrypt the specifics and hide the title if necessary.
  if (cryptographer_) {
    // If there is a cryptographer and CanCommitItems() is true then the
    // cryptographer is valid and ready to encrypt.
    sync_pb::EntitySpecifics encrypted_specifics;
    bool result = cryptographer_->Encrypt(
        sync_entity->specifics(), encrypted_specifics.mutable_encrypted());
    DCHECK(result);
    sync_entity->mutable_specifics()->CopyFrom(encrypted_specifics);
    sync_entity->set_name("encrypted");
  }

  // Always include enough specifics to identify the type. Do this even in
  // deletion requests, where the specifics are otherwise invalid.
  AddDefaultFieldValue(type_, sync_entity->mutable_specifics());

  // TODO(crbug.com/516866): Set parent_id_string for hierarchical types here.
}

void ModelTypeWorker::OnCryptographerUpdated() {
  DCHECK(cryptographer_);
  UpdateEncryptionKeyName();
  DecryptedStoredEntities();
}

bool ModelTypeWorker::UpdateEncryptionKeyName() {
  const std::string& new_key_name = cryptographer_->GetDefaultNigoriKeyName();
  const std::string& old_key_name = model_type_state_.encryption_key_name();
  if (old_key_name == new_key_name) {
    return false;
  }

  DVLOG(1) << ModelTypeToString(type_) << ": Updating encryption key "
           << old_key_name << " -> " << new_key_name;
  model_type_state_.set_encryption_key_name(new_key_name);
  return true;
}

void ModelTypeWorker::DecryptedStoredEntities() {
  has_encrypted_updates_ = false;
  for (const auto& kv : entities_) {
    WorkerEntityTracker* entity = kv.second.get();
    if (entity->HasEncryptedUpdate()) {
      const UpdateResponseData& encrypted_update = entity->GetEncryptedUpdate();
      const EntityData& data = encrypted_update.entity.value();
      DCHECK(data.specifics.has_encrypted());

      if (cryptographer_->CanDecrypt(data.specifics.encrypted())) {
        EntityData decrypted_data;
        if (DecryptSpecifics(data.specifics, &decrypted_data.specifics)) {
          // Copy other fields one by one since EntityData doesn't allow
          // copying.
          decrypted_data.id = data.id;
          decrypted_data.client_tag_hash = data.client_tag_hash;
          decrypted_data.non_unique_name = data.non_unique_name;
          decrypted_data.creation_time = data.creation_time;
          decrypted_data.modification_time = data.modification_time;

          UpdateResponseData decrypted_update;
          decrypted_update.entity = decrypted_data.PassToPtr();
          decrypted_update.response_version = encrypted_update.response_version;
          decrypted_update.encryption_key_name =
              data.specifics.encrypted().key_name();
          pending_updates_.push_back(decrypted_update);

          entity->ClearEncryptedUpdate();
        }
      } else {
        has_encrypted_updates_ = true;
      }
    }
  }
}

bool ModelTypeWorker::DecryptSpecifics(const sync_pb::EntitySpecifics& in,
                                       sync_pb::EntitySpecifics* out) {
  DCHECK(cryptographer_);
  DCHECK(in.has_encrypted());
  DCHECK(cryptographer_->CanDecrypt(in.encrypted()));

  std::string plaintext = cryptographer_->DecryptToString(in.encrypted());
  if (plaintext.empty()) {
    LOG(ERROR) << "Failed to decrypt a decryptable entity";
    return false;
  }
  if (!out->ParseFromString(plaintext)) {
    LOG(ERROR) << "Failed to parse decrypted entity";
    return false;
  }
  return true;
}

WorkerEntityTracker* ModelTypeWorker::GetEntityTracker(
    const std::string& tag_hash) {
  auto it = entities_.find(tag_hash);
  return it != entities_.end() ? it->second.get() : nullptr;
}

WorkerEntityTracker* ModelTypeWorker::CreateEntityTracker(
    const EntityData& data) {
  DCHECK(entities_.find(data.client_tag_hash) == entities_.end());
  std::unique_ptr<WorkerEntityTracker> entity =
      base::MakeUnique<WorkerEntityTracker>(data.client_tag_hash);
  WorkerEntityTracker* entity_ptr = entity.get();
  entities_[data.client_tag_hash] = std::move(entity);
  return entity_ptr;
}

WorkerEntityTracker* ModelTypeWorker::GetOrCreateEntityTracker(
    const EntityData& data) {
  WorkerEntityTracker* entity = GetEntityTracker(data.client_tag_hash);
  return entity ? entity : CreateEntityTracker(data);
}

}  // namespace syncer
