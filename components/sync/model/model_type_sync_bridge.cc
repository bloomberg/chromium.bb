// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_sync_bridge.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/sync/model/metadata_batch.h"

namespace syncer {

ModelTypeSyncBridge::ModelTypeSyncBridge(
    const ChangeProcessorFactory& change_processor_factory,
    ModelType type)
    : type_(type),
      change_processor_factory_(change_processor_factory),
      change_processor_(change_processor_factory_.Run(type_, this)) {}

ModelTypeSyncBridge::~ModelTypeSyncBridge() {}

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
  DCHECK(change_processor_);
  change_processor_->DisableSync();
  change_processor_ = change_processor_factory_.Run(type_, this);
  // DisableSync() should delete all metadata, so it'll be safe to tell the new
  // processor that there is no metadata. DisableSync() should never be called
  // while the models are loading, aka before the service has finished loading
  // the initial metadata.
  change_processor_->ModelReadyToSync(base::MakeUnique<MetadataBatch>());
}

ModelTypeChangeProcessor* ModelTypeSyncBridge::change_processor() const {
  return change_processor_.get();
}

}  // namespace syncer
