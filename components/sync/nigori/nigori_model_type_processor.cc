// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_model_type_processor.h"

#include "components/sync/engine/commit_queue.h"

namespace syncer {

NigoriModelTypeProcessor::NigoriModelTypeProcessor() : bridge_(nullptr) {}

NigoriModelTypeProcessor::~NigoriModelTypeProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriModelTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
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
    std::pair<sync_pb::ModelTypeState, sync_pb::EntityMetadata>
        nigori_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(bridge);
  bridge_ = bridge;
  NOTIMPLEMENTED();
}

void NigoriModelTypeProcessor::Put(std::unique_ptr<EntityData> entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

std::pair<sync_pb::ModelTypeState, sync_pb::EntityMetadata>
NigoriModelTypeProcessor::GetMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return std::pair<sync_pb::ModelTypeState, sync_pb::EntityMetadata>(
      sync_pb::ModelTypeState(), sync_pb::EntityMetadata());
}

}  // namespace syncer
