// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/syncable_service_adapter.h"

#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/api/sync_data.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"

namespace browser_sync {

SyncableServiceAdapter::SyncableServiceAdapter(
    syncable::ModelType type,
    SyncableService* service,
    GenericChangeProcessor* sync_processor)
    : syncing_(false),
      type_(type),
      service_(service),
      sync_processor_(sync_processor) {
}

SyncableServiceAdapter::~SyncableServiceAdapter() {
  if (syncing_) {
    NOTREACHED();
    LOG(ERROR) << "SyncableServiceAdapter for "
               << syncable::ModelTypeToString(type_) << " destroyed before "
               << "without being shut down properly.";
    service_->StopSyncing(type_);
  }
}

bool SyncableServiceAdapter::AssociateModels() {
  syncing_ = true;
  SyncDataList initial_sync_data;
  if (!sync_processor_->GetSyncDataForType(type_, &initial_sync_data)) {
    return false;
  }
  return service_->MergeDataAndStartSyncing(type_,
                                            initial_sync_data,
                                            sync_processor_);
}

bool SyncableServiceAdapter::DisassociateModels() {
  service_->StopSyncing(type_);
  syncing_ = false;
  return true;
}

bool SyncableServiceAdapter::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  return sync_processor_->SyncModelHasUserCreatedNodes(type_, has_nodes);
}

void SyncableServiceAdapter::AbortAssociation() {
  service_->StopSyncing(type_);
  syncing_ = false;
}

bool SyncableServiceAdapter::CryptoReadyIfNecessary() {
  return sync_processor_->CryptoReadyIfNecessary(type_);
}

}  // namespace browser_sync
