// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/generic_change_processor.h"

#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/syncable_service.h"

namespace browser_sync {

GenericChangeProcessor::GenericChangeProcessor(
    SyncableService* local_service,
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      local_service_(local_service) {
  DCHECK(local_service_);
}
GenericChangeProcessor::~GenericChangeProcessor() {
  // Set to null to ensure it's not used after destruction.
  local_service_ = NULL;
}

void GenericChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  local_service_->ApplyChangesFromSync(trans, changes, change_count);
}

void GenericChangeProcessor::StartImpl(Profile* profile) {}

void GenericChangeProcessor::StopImpl() {}

}  // namespace browser_sync
