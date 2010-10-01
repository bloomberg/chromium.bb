// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {
namespace sessions {

SyncSession::SyncSession(SyncSessionContext* context, Delegate* delegate)
    : context_(context),
      source_(sync_pb::GetUpdatesCallerInfo::UNKNOWN),
      write_transaction_(NULL),
      delegate_(delegate) {

  context_->registrar()->GetWorkers(
      const_cast<std::vector<ModelSafeWorker*>*>(&workers_));
  context_->registrar()->GetModelSafeRoutingInfo(
      const_cast<ModelSafeRoutingInfo*>(&routing_info_));
  status_controller_.reset(new StatusController(routing_info_));
}

SyncSession::~SyncSession() {}

SyncSessionSnapshot SyncSession::TakeSnapshot() const {
  syncable::ScopedDirLookup dir(context_->directory_manager(),
                                context_->account_name());
  if (!dir.good())
    LOG(ERROR) << "Scoped dir lookup failed!";

  bool is_share_useable = true;
  syncable::ModelTypeBitSet initial_sync_ended;
  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType type(syncable::ModelTypeFromInt(i));
    if (routing_info_.count(type) != 0) {
      if (dir->initial_sync_ended_for_type(type))
        initial_sync_ended.set(type);
      else
        is_share_useable = false;
    }
  }

  return SyncSessionSnapshot(
      status_controller_->syncer_status(),
      status_controller_->error_counters(),
      status_controller_->num_server_changes_remaining(),
      status_controller_->ComputeMaxLocalTimestamp(),
      is_share_useable,
      initial_sync_ended,
      HasMoreToSync(),
      delegate_->IsSyncingCurrentlySilenced(),
      status_controller_->unsynced_handles().size(),
      status_controller_->TotalNumConflictingItems(),
      status_controller_->did_commit_items());
}

sync_pb::GetUpdatesCallerInfo::GetUpdatesSource
    SyncSession::TestAndSetSource() {
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource old_source = source_;
  set_source(sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION);
  return old_source;
}

bool SyncSession::HasMoreToSync() const {
  const StatusController* status = status_controller_.get();
  return ((status->commit_ids().size() < status->unsynced_handles().size()) &&
      status->syncer_status().num_successful_commits > 0) ||
      status->conflict_sets_built() ||
      status->conflicts_resolved();
      // Or, we have conflicting updates, but we're making progress on
      // resolving them...
  }

}  // namespace sessions
}  // namespace browser_sync
