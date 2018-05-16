// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_sync_bridge.h"

#include <utility>

#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"

namespace syncer {

ModelTypeSyncBridge::ModelTypeSyncBridge(
    std::unique_ptr<ModelTypeChangeProcessor> change_processor)
    : change_processor_(std::move(change_processor)) {
  DCHECK(change_processor_);
  change_processor_->OnModelStarting(this);
}

ModelTypeSyncBridge::~ModelTypeSyncBridge() {}

void ModelTypeSyncBridge::OnSyncStarting() {}

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

ModelTypeSyncBridge::DisableSyncResponse
ModelTypeSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  // Nothing to do if this fails, so just ignore the error it might return.
  ApplySyncChanges(std::move(delete_metadata_change_list), EntityChangeList());
  return DisableSyncResponse::kModelStillReadyToSync;
}

ModelTypeChangeProcessor* ModelTypeSyncBridge::change_processor() {
  return change_processor_.get();
}

}  // namespace syncer
