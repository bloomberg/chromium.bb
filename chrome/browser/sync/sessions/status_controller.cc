// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/status_controller.h"

#include "base/basictypes.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {
namespace sessions {

using syncable::FIRST_REAL_MODEL_TYPE;
using syncable::MODEL_TYPE_COUNT;

StatusController::StatusController(const ModelSafeRoutingInfo& routes)
    : shared_(&is_dirty_),
      per_model_group_deleter_(&per_model_group_),
      per_model_type_deleter_(&per_model_type_),
      is_dirty_(false),
      group_restriction_in_effect_(false),
      group_restriction_(GROUP_PASSIVE),
      routing_info_(routes) {
}

StatusController::~StatusController() {}

bool StatusController::TestAndClearIsDirty() {
  bool is_dirty = is_dirty_;
  is_dirty_ = false;
  return is_dirty;
}

PerModelSafeGroupState* StatusController::GetOrCreateModelSafeGroupState(
    bool restrict, ModelSafeGroup group) {
  DCHECK(restrict == group_restriction_in_effect_) << "Group violation!";
  if (per_model_group_.find(group) == per_model_group_.end()) {
    PerModelSafeGroupState* state = new PerModelSafeGroupState(&is_dirty_);
    per_model_group_[group] = state;
    return state;
  }
  return per_model_group_[group];
}

PerModelTypeState* StatusController::GetOrCreateModelTypeState(
    bool restrict, syncable::ModelType model) {
  if (restrict) {
    DCHECK(group_restriction_in_effect_) << "No group restriction in effect!";
    DCHECK_EQ(group_restriction_, GetGroupForModelType(model, routing_info_));
  }
  if (per_model_type_.find(model) == per_model_type_.end()) {
    PerModelTypeState* state = new PerModelTypeState(&is_dirty_);
    per_model_type_[model] = state;
    return state;
  }
  return per_model_type_[model];
}

void StatusController::increment_num_conflicting_commits_by(int value) {
  if (value == 0)
    return;
  shared_.error_counters.mutate()->num_conflicting_commits += value;
}

void StatusController::reset_num_conflicting_commits() {
  if (shared_.error_counters.value().num_conflicting_commits != 0)
    shared_.error_counters.mutate()->num_conflicting_commits = 0;
}

void StatusController::set_num_consecutive_transient_error_commits(int value) {
  if (shared_.error_counters.value().consecutive_transient_error_commits !=
      value) {
    shared_.error_counters.mutate()->consecutive_transient_error_commits =
        value;
  }
}

void StatusController::increment_num_consecutive_transient_error_commits_by(
    int value) {
  set_num_consecutive_transient_error_commits(
      shared_.error_counters.value().consecutive_transient_error_commits +
      value);
}

void StatusController::set_num_consecutive_errors(int value) {
  if (shared_.error_counters.value().consecutive_errors != value)
    shared_.error_counters.mutate()->consecutive_errors = value;
}

void StatusController::set_current_download_timestamp(
    syncable::ModelType model,
    int64 current_timestamp) {
  PerModelTypeState* state = GetOrCreateModelTypeState(false, model);
  if (current_timestamp > state->current_download_timestamp.value())
    *(state->current_download_timestamp.mutate()) = current_timestamp;
}

void StatusController::set_num_server_changes_remaining(
    int64 changes_remaining) {
  if (shared_.num_server_changes_remaining.value() != changes_remaining)
    *(shared_.num_server_changes_remaining.mutate()) = changes_remaining;
}

void StatusController::set_invalid_store(bool invalid_store) {
  if (shared_.syncer_status.value().invalid_store != invalid_store)
    shared_.syncer_status.mutate()->invalid_store = invalid_store;
}

void StatusController::set_syncer_stuck(bool syncer_stuck) {
  if (shared_.syncer_status.value().syncer_stuck != syncer_stuck)
    shared_.syncer_status.mutate()->syncer_stuck = syncer_stuck;
}

void StatusController::set_syncing(bool syncing) {
  if (shared_.syncer_status.value().syncing != syncing)
    shared_.syncer_status.mutate()->syncing = syncing;
}

void StatusController::set_num_successful_bookmark_commits(int value) {
  if (shared_.syncer_status.value().num_successful_bookmark_commits != value)
    shared_.syncer_status.mutate()->num_successful_bookmark_commits = value;
}

void StatusController::set_unsynced_handles(
    const std::vector<int64>& unsynced_handles) {
  if (!operator==(unsynced_handles, shared_.unsynced_handles.value())) {
    *(shared_.unsynced_handles.mutate()) = unsynced_handles;
  }
}

void StatusController::increment_num_consecutive_errors() {
  set_num_consecutive_errors(
      shared_.error_counters.value().consecutive_errors + 1);
}

void StatusController::increment_num_consecutive_errors_by(int value) {
  set_num_consecutive_errors(
      shared_.error_counters.value().consecutive_errors + value);
}

void StatusController::increment_num_successful_bookmark_commits() {
  set_num_successful_bookmark_commits(
      shared_.syncer_status.value().num_successful_bookmark_commits + 1);
}

void StatusController::increment_num_successful_commits() {
  shared_.syncer_status.mutate()->num_successful_commits++;
}

void StatusController::set_commit_set(const OrderedCommitSet& commit_set) {
  DCHECK(!group_restriction_in_effect_);
  shared_.commit_set = commit_set;
}

void StatusController::update_conflict_sets_built(bool built) {
  shared_.control_params.conflict_sets_built |= built;
}
void StatusController::update_conflicts_resolved(bool resolved) {
  shared_.control_params.conflict_sets_built |= resolved;
}
void StatusController::reset_conflicts_resolved() {
  shared_.control_params.conflicts_resolved = false;
}
void StatusController::set_items_committed() {
  shared_.control_params.items_committed = true;
}

// Returns the number of updates received from the sync server.
int64 StatusController::CountUpdates() const {
  const ClientToServerResponse& updates = shared_.updates_response;
  if (updates.has_get_updates()) {
    return updates.get_updates().entries().size();
  } else {
    return 0;
  }
}

bool StatusController::CurrentCommitIdProjectionHasIndex(size_t index) {
  OrderedCommitSet::Projection proj =
      shared_.commit_set.GetCommitIdProjection(group_restriction_);
  return std::binary_search(proj.begin(), proj.end(), index);
}

int64 StatusController::ComputeMaxLocalTimestamp() const {
  std::map<syncable::ModelType, PerModelTypeState*>::const_iterator it =
      per_model_type_.begin();
  int64 max_timestamp = 0;
  for (; it != per_model_type_.end(); ++it) {
    if (it->second->current_download_timestamp.value() > max_timestamp)
      max_timestamp = it->second->current_download_timestamp.value();
  }
  return max_timestamp;
}

bool StatusController::HasConflictingUpdates() const {
  DCHECK(!group_restriction_in_effect_)
      << "HasConflictingUpdates applies to all ModelSafeGroups";
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::const_iterator it =
    per_model_group_.begin();
  for (; it != per_model_group_.end(); ++it) {
    if (it->second->update_progress.HasConflictingUpdates())
      return true;
  }
  return false;
}

int StatusController::TotalNumConflictingItems() const {
  DCHECK(!group_restriction_in_effect_)
      << "TotalNumConflictingItems applies to all ModelSafeGroups";
  std::map<ModelSafeGroup, PerModelSafeGroupState*>::const_iterator it =
    per_model_group_.begin();
  int sum = 0;
  for (; it != per_model_group_.end(); ++it) {
    sum += it->second->conflict_progress.ConflictingItemsSize();
  }
  return sum;
}

bool StatusController::ServerSaysNothingMoreToDownload() const {
  if (!download_updates_succeeded())
    return false;
  // If we didn't request every enabled datatype, then we can't say for
  // sure that there's nothing left to download.
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    if (!updates_request_parameters().data_types[i] &&
        routing_info_.count(syncable::ModelTypeFromInt(i)) != 0) {
      return false;
    }
  }
  // Changes remaining is an estimate, but if it's estimated to be
  // zero, that's firm and we don't have to ask again.
  if (updates_response().get_updates().has_changes_remaining() &&
      updates_response().get_updates().changes_remaining() == 0) {
    return true;
  }
  // Otherwise, the server can also indicate "you're up to date"
  // by not sending a new timestamp.
  return !updates_response().get_updates().has_new_timestamp();
}

}  // namespace sessions
}  // namespace browser_sync
