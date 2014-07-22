// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/shared_change_processor_ref.h"

namespace sync_driver {

SharedChangeProcessorRef::SharedChangeProcessorRef(
    const scoped_refptr<SharedChangeProcessor>& change_processor)
    : change_processor_(change_processor) {
  DCHECK(change_processor_.get());
}

SharedChangeProcessorRef::~SharedChangeProcessorRef() {}

syncer::SyncError SharedChangeProcessorRef::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  return change_processor_->ProcessSyncChanges(from_here, change_list);
}

syncer::SyncDataList SharedChangeProcessorRef::GetAllSyncData(
    syncer::ModelType type) const {
  return change_processor_->GetAllSyncData(type);
}

syncer::SyncError SharedChangeProcessorRef::UpdateDataTypeContext(
    syncer::ModelType type,
    syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
    const std::string& context) {
  return change_processor_->UpdateDataTypeContext(
      type, refresh_status, context);
}

syncer::SyncError SharedChangeProcessorRef::CreateAndUploadError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  return change_processor_->CreateAndUploadError(from_here, message);
}

}  // namespace sync_driver
