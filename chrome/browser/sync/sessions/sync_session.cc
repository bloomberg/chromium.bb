// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"

namespace browser_sync {
namespace sessions {

SyncSession::SyncSession(SyncSessionContext* context, Delegate* delegate)
    : context_(context),
      source_(sync_pb::GetUpdatesCallerInfo::UNKNOWN),
      write_transaction_(NULL),
      delegate_(delegate),
      auth_failure_occurred_(false) {
}

SyncSessionSnapshot SyncSession::TakeSnapshot() const {
  syncable::ScopedDirLookup dir(context_->directory_manager(),
                                context_->account_name());
  if (!dir.good())
    LOG(ERROR) << "Scoped dir lookup failed!";

  const bool is_share_usable = dir->initial_sync_ended();
  return SyncSessionSnapshot(
      status_controller_.syncer_status(),
      status_controller_.error_counters(),
      status_controller_.change_progress(),
      is_share_usable,
      HasMoreToSync(),
      delegate_->IsSyncingCurrentlySilenced(),
      status_controller_.unsynced_handles().size(),
      status_controller_.conflict_progress()->ConflictingItemsSize(),
      status_controller_.did_commit_items());
}

sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE
    SyncSession::TestAndSetSource() {
  sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE old_source = source_;
  set_source(sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION);
  return old_source;
}

bool SyncSession::HasMoreToSync() const {
  const StatusController& status = status_controller_;
  return ((status.commit_ids().size() < status.unsynced_handles().size()) &&
      status.syncer_status().num_successful_commits > 0) ||
      status.conflict_sets_built() ||
      status.conflicts_resolved() ||
      // Or, we have conflicting updates, but we're making progress on
      // resolving them...
      !status.got_zero_updates() ||
      status.timestamp_dirty();
}

}  // namespace sessions
}  // namespace browser_sync
