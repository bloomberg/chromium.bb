// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_sync_bridge.h"

#include <utility>

#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"

namespace syncer {

ModelTypeSyncBridge::ModelTypeSyncBridge(
    std::unique_ptr<ModelTypeChangeProcessor> change_processor)
    : change_processor_(std::move(change_processor)) {
  DCHECK(change_processor_);
}

ModelTypeSyncBridge::~ModelTypeSyncBridge() {}

bool ModelTypeSyncBridge::SupportsGetStorageKey() const {
  return true;
}

ConflictResolution ModelTypeSyncBridge::ResolveConflict(
    const EntityData& local_data,
    const EntityData& remote_data) const {
  if (remote_data.is_deleted()) {
    DCHECK(!local_data.is_deleted());
    return ConflictResolution::UseLocal();
  }
  return ConflictResolution::UseRemote();
}

void ModelTypeSyncBridge::OnSyncStarting(
    const ModelErrorHandler& error_handler,
    const ModelTypeChangeProcessor::StartCallback& start_callback) {
  change_processor_->OnSyncStarting(std::move(error_handler), start_callback);
}

void ModelTypeSyncBridge::DisableSync() {
  // The processor resets its internal state and clears the metadata (by calling
  // ApplyDisableSyncChanges() of this bridge).
  change_processor_->DisableSync();
}

void ModelTypeSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  // Nothing to do if this fails, so just ignore the error it might return.
  ApplySyncChanges(std::move(delete_metadata_change_list), EntityChangeList());
}

ModelTypeChangeProcessor* ModelTypeSyncBridge::change_processor() const {
  return change_processor_.get();
}

}  // namespace syncer
