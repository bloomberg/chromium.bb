// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_session.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {
namespace sessions {

namespace {

std::set<ModelSafeGroup> ComputeEnabledGroups(
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers) {
  std::set<ModelSafeGroup> enabled_groups;
  // Project the list of enabled types (i.e., types in the routing
  // info) to a list of enabled groups.
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    enabled_groups.insert(it->second);
  }
  // GROUP_PASSIVE is always enabled, since that's the group that
  // top-level folders map to.
  enabled_groups.insert(GROUP_PASSIVE);
  if (DCHECK_IS_ON()) {
    // We sometimes create dummy SyncSession objects (see
    // SyncScheduler::InitialSnapshot) so don't check in that case.
    if (!routing_info.empty() || !workers.empty()) {
      std::set<ModelSafeGroup> groups_with_workers;
      for (std::vector<ModelSafeWorker*>::const_iterator it = workers.begin();
           it != workers.end(); ++it) {
        groups_with_workers.insert((*it)->GetModelSafeGroup());
      }
      // All enabled groups should have a corresponding worker.
      DCHECK(std::includes(
          groups_with_workers.begin(), groups_with_workers.end(),
          enabled_groups.begin(), enabled_groups.end()));
    }
  }
  return enabled_groups;
}

}  // namesepace

SyncSession::SyncSession(SyncSessionContext* context, Delegate* delegate,
    const SyncSourceInfo& source,
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers)
    : context_(context),
      source_(source),
      write_transaction_(NULL),
      delegate_(delegate),
      workers_(workers),
      routing_info_(routing_info),
      enabled_groups_(ComputeEnabledGroups(routing_info_, workers_)) {
  status_controller_.reset(new StatusController(routing_info_));
  std::sort(workers_.begin(), workers_.end());
}

SyncSession::~SyncSession() {}

void SyncSession::Coalesce(const SyncSession& session) {
  if (context_ != session.context() || delegate_ != session.delegate_) {
    NOTREACHED();
    return;
  }

  // When we coalesce sessions, the sync update source gets overwritten with the
  // most recent, while the type/payload map gets merged.
  CoalescePayloads(&source_.types, session.source_.types);
  source_.updates_source = session.source_.updates_source;

  std::vector<ModelSafeWorker*> temp;
  std::set_union(workers_.begin(), workers_.end(),
                 session.workers_.begin(), session.workers_.end(),
                 std::back_inserter(temp));
  workers_.swap(temp);

  // We have to update the model safe routing info to the union. In case the
  // same key is present in both pick the one from session.
  for (ModelSafeRoutingInfo::const_iterator it =
       session.routing_info_.begin();
       it != session.routing_info_.end();
       ++it) {
    routing_info_[it->first] = it->second;
  }

  // Now update enabled groups.
  enabled_groups_ = ComputeEnabledGroups(routing_info_, workers_);
}

void SyncSession::RebaseRoutingInfoWithLatest(const SyncSession& session) {
  ModelSafeRoutingInfo temp_routing_info;

  // Take the intersecion and also set the routing info(it->second) from the
  // passed in session.
  for (ModelSafeRoutingInfo::const_iterator it =
       session.routing_info_.begin(); it != session.routing_info_.end();
       ++it) {
    if (routing_info_.find(it->first) != routing_info_.end()) {
      temp_routing_info[it->first] = it->second;
    }
  }

  // Now swap it.
  routing_info_.swap(temp_routing_info);

  // Now update the payload map.
  PurgeStalePayload(&source_.types, session.routing_info_);

  // Now update the workers.
  std::vector<ModelSafeWorker*> temp;
  std::set_intersection(workers_.begin(), workers_.end(),
                 session.workers_.begin(), session.workers_.end(),
                 std::back_inserter(temp));
  workers_.swap(temp);

  // Now update enabled groups.
  enabled_groups_ = ComputeEnabledGroups(routing_info_, workers_);
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
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType type(syncable::ModelTypeFromInt(i));
    if (routing_info_.count(type) != 0) {
      if (dir->initial_sync_ended_for_type(type))
        initial_sync_ended.set(type);
      else
        is_share_useable = false;
    }
    dir->GetDownloadProgressAsString(type, &download_progress_markers[i]);
  }

  return SyncSessionSnapshot(
      status_controller_->syncer_status(),
      status_controller_->error(),
      status_controller_->num_server_changes_remaining(),
      is_share_useable,
      initial_sync_ended,
      download_progress_markers,
      HasMoreToSync(),
      delegate_->IsSyncingCurrentlySilenced(),
      status_controller_->unsynced_handles().size(),
      status_controller_->TotalNumBlockingConflictingItems(),
      status_controller_->TotalNumConflictingItems(),
      status_controller_->did_commit_items(),
      source_,
      dir->GetEntriesCount(),
      status_controller_->sync_start_time());
}

SyncSourceInfo SyncSession::TestAndSetSource() {
  SyncSourceInfo old_source = source_;
  source_ = SyncSourceInfo(
      sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION,
      source_.types);
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

const std::set<ModelSafeGroup>& SyncSession::GetEnabledGroups() const {
  return enabled_groups_;
}

std::set<ModelSafeGroup> SyncSession::GetEnabledGroupsWithConflicts() const {
  const std::set<ModelSafeGroup>& enabled_groups = GetEnabledGroups();
  std::set<ModelSafeGroup> enabled_groups_with_conflicts;
  for (std::set<ModelSafeGroup>::const_iterator it =
           enabled_groups.begin(); it != enabled_groups.end(); ++it) {
    const sessions::ConflictProgress* conflict_progress =
        status_controller_->GetUnrestrictedConflictProgress(*it);
    if (conflict_progress &&
        (conflict_progress->ConflictingItemsBegin() !=
         conflict_progress->ConflictingItemsEnd())) {
      enabled_groups_with_conflicts.insert(*it);
    }
  }
  return enabled_groups_with_conflicts;
}

std::set<ModelSafeGroup>
    SyncSession::GetEnabledGroupsWithVerifiedUpdates() const {
  const std::set<ModelSafeGroup>& enabled_groups = GetEnabledGroups();
  std::set<ModelSafeGroup> enabled_groups_with_verified_updates;
  for (std::set<ModelSafeGroup>::const_iterator it =
           enabled_groups.begin(); it != enabled_groups.end(); ++it) {
    const UpdateProgress* update_progress =
        status_controller_->GetUnrestrictedUpdateProgress(*it);
    if (update_progress &&
        (update_progress->VerifiedUpdatesBegin() !=
         update_progress->VerifiedUpdatesEnd())) {
      enabled_groups_with_verified_updates.insert(*it);
    }
  }

  return enabled_groups_with_verified_updates;
}


}  // namespace sessions
}  // namespace browser_sync
