// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {
namespace sessions {

SyncSession::SyncSession(SyncSessionContext* context, Delegate* delegate,
    SyncSourceInfo source,
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers)
    : context_(context),
      source_(source),
      write_transaction_(NULL),
      delegate_(delegate),
      workers_(workers),
      routing_info_(routing_info) {
  status_controller_.reset(new StatusController(routing_info_));
}

SyncSession::~SyncSession() {}

void SyncSession::Coalesce(const SyncSession& session) {
  if (context_ != session.context() || delegate_ != session.delegate_) {
    NOTREACHED();
    return;
  }

  source_ = SyncSourceInfo(session.source_.first,
                           source_.second | session.source_.second);

  std::vector<ModelSafeWorker*> temp;
  std::set_union(workers_.begin(), workers_.end(),
                 session.workers_.begin(), session.workers_.end(),
                 std::back_inserter(temp));
  workers_.swap(temp);

  ModelSafeRoutingInfo temp_r;
  std::set_union(routing_info_.begin(), routing_info_.end(),
      session.routing_info_.begin(), session.routing_info_.end(),
      std::insert_iterator<ModelSafeRoutingInfo>(temp_r, temp_r.begin()));
  routing_info_.swap(temp_r);
}

void SyncSession::ResetTransientState() {
  status_controller_.reset(new StatusController(routing_info_));
}

SyncSessionSnapshot SyncSession::TakeSnapshot() const {
  syncable::ScopedDirLookup dir(context_->directory_manager(),
                                context_->account_name());
  if (!dir.good())
    LOG(ERROR) << "Scoped dir lookup failed!";

  bool is_share_useable = true;
  syncable::ModelTypeBitSet initial_sync_ended;
  std::string download_progress_markers[syncable::MODEL_TYPE_COUNT];
  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType type(syncable::ModelTypeFromInt(i));
    if (routing_info_.count(type) != 0) {
      if (dir->initial_sync_ended_for_type(type))
        initial_sync_ended.set(type);
      else
        is_share_useable = false;
      dir->GetDownloadProgressAsString(type, &download_progress_markers[i]);
    }
  }

  return SyncSessionSnapshot(
      status_controller_->syncer_status(),
      status_controller_->error_counters(),
      status_controller_->num_server_changes_remaining(),
      is_share_useable,
      initial_sync_ended,
      download_progress_markers,
      HasMoreToSync(),
      delegate_->IsSyncingCurrentlySilenced(),
      status_controller_->unsynced_handles().size(),
      status_controller_->TotalNumConflictingItems(),
      status_controller_->did_commit_items(),
      source_);
}

SyncSourceInfo SyncSession::TestAndSetSource() {
  SyncSourceInfo old_source = source_;
  source_ = SyncSourceInfo(
      sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION,
      source_.second);
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
