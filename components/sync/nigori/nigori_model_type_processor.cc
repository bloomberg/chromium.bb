// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_model_type_processor.h"

#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/model_impl/processor_entity.h"
#include "components/sync/nigori/nigori_sync_bridge.h"

namespace syncer {

namespace {

// TODO(mamir): remove those and adjust the code accordingly.
const char kNigoriStorageKey[] = "NigoriStorageKey";
const char kNigoriClientTagHash[] = "NigoriClientTagHash";

}  // namespace

NigoriModelTypeProcessor::NigoriModelTypeProcessor() : bridge_(nullptr) {}

NigoriModelTypeProcessor::~NigoriModelTypeProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriModelTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Successfully connected Encryption Keys";

  worker_ = std::move(worker);
  NudgeForCommitIfNeeded();
}

void NigoriModelTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsConnected());

  DVLOG(1) << "Disconnecting sync for Encryption Keys";
  // TODO(mamir): weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
  worker_.reset();
  if (entity_) {
    entity_->ClearTransientSyncState();
  }
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(max_entries, 0U);
  // If there is a model error, it must have been reported already but hasn't
  // reached the sync engine yet. In this case return directly to avoid
  // interactions with the bridge.
  if (model_error_) {
    std::move(callback).Run(CommitRequestDataList());
    return;
  }

  DCHECK(entity_);

  // No local changes to commit.
  if (!entity_->RequiresCommitRequest()) {
    std::move(callback).Run(CommitRequestDataList());
    return;
  }

  if (entity_->RequiresCommitData()) {
    // SetCommitData will update EntityData's fields with values from
    // metadata.
    entity_->SetCommitData(bridge_->GetData());
  }

  auto commit_request_data = std::make_unique<CommitRequestData>();
  entity_->InitializeCommitRequestData(commit_request_data.get());

  CommitRequestDataList commit_request_data_list;
  commit_request_data_list.push_back(std::move(commit_request_data));

  std::move(callback).Run(std::move(commit_request_data_list));
}

void NigoriModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const CommitResponseDataList& response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& type_state,
    UpdateResponseDataList updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::GetStatusCountersForDebugging(
    StatusCountersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::RecordMemoryUsageAndCountsHistograms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::ModelReadyToSync(
    NigoriSyncBridge* bridge,
    NigoriMetadataBatch nigori_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(bridge);
  DCHECK(!model_ready_to_sync_);
  bridge_ = bridge;
  model_ready_to_sync_ = true;

  // Abort if the model already experienced an error.
  if (model_error_) {
    return;
  }

  if (nigori_metadata.model_type_state.initial_sync_done() &&
      nigori_metadata.entity_metadata) {
    model_type_state_ = std::move(nigori_metadata.model_type_state);
    sync_pb::EntityMetadata metadata =
        std::move(*nigori_metadata.entity_metadata);
    metadata.set_client_tag_hash(kNigoriClientTagHash);
    entity_ = ProcessorEntity::CreateFromMetadata(kNigoriStorageKey,
                                                  std::move(metadata));
  } else {
    // First time syncing or persisted data are corrupted; initialize metadata.
    model_type_state_.mutable_progress_marker()->set_data_type_id(
        sync_pb::EntitySpecifics::kNigoriFieldNumber);
  }
  // TODO(mamir): ConnectIfReady();
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::Put(std::unique_ptr<EntityData> entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entity_data);
  DCHECK(!entity_data->is_deleted());
  DCHECK(!entity_data->is_folder);
  DCHECK(!entity_data->non_unique_name.empty());
  DCHECK(!entity_data->specifics.has_encrypted());
  DCHECK_EQ(NIGORI, GetModelTypeFromSpecifics(entity_data->specifics));
  DCHECK(entity_);

  if (!model_type_state_.initial_sync_done()) {
    // Ignore changes before the initial sync is done.
    return;
  }

  if (entity_->MatchesData(*entity_data)) {
    // Ignore changes that don't actually change anything.
    return;
  }

  entity_->MakeLocalChange(std::move(entity_data));
  NudgeForCommitIfNeeded();
}

NigoriMetadataBatch NigoriModelTypeProcessor::GetMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsTrackingMetadata());
  DCHECK(entity_);

  NigoriMetadataBatch nigori_metadata_batch;
  nigori_metadata_batch.model_type_state = model_type_state_;
  nigori_metadata_batch.entity_metadata = entity_->metadata();

  return nigori_metadata_batch;
}

bool NigoriModelTypeProcessor::IsTrackingMetadata() {
  return model_type_state_.initial_sync_done();
}

bool NigoriModelTypeProcessor::IsConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_ != nullptr;
}

void NigoriModelTypeProcessor::NudgeForCommitIfNeeded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't bother sending anything if there's no one to send to.
  if (!IsConnected()) {
    return;
  }

  // Don't send anything if the type is not ready to handle commits.
  if (!model_type_state_.initial_sync_done()) {
    return;
  }

  // Nudge worker if there are any entities with local changes.
  if (entity_->RequiresCommitRequest()) {
    worker_->NudgeForCommit();
  }
}

}  // namespace syncer
