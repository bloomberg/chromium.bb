// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/status_controller.h"

#include "base/basictypes.h"

namespace browser_sync {
namespace sessions {

StatusController::StatusController()
    : commit_set_(ModelSafeRoutingInfo()),
      transient_(new Dirtyable<TransientState>()),
      group_restriction_in_effect_(false),
      group_restriction_(GROUP_PASSIVE) {}

// Helper to set the 'dirty' bit in the container of the field being
// mutated.
template <typename FieldType, typename DirtyableContainer>
void SetAndMarkDirtyIfChanged(FieldType* old_value,
    const FieldType& new_value, DirtyableContainer* container) {
  if (new_value != *old_value)
    container->set_dirty();
  *old_value = new_value;
}

template <typename T>
bool StatusController::Dirtyable<T>::TestAndClearIsDirty() {
  bool dirty = dirty_;
  dirty_ = false;
  return dirty;
}

bool StatusController::TestAndClearIsDirty() {
  bool is_dirty = change_progress_.TestAndClearIsDirty() ||
      syncer_status_.TestAndClearIsDirty() ||
      error_counters_.TestAndClearIsDirty() ||
      transient_->TestAndClearIsDirty() ||
      conflict_progress_.progress_changed();
  conflict_progress_.reset_progress_changed();
  return is_dirty;
}
void StatusController::increment_num_conflicting_commits_by(int value) {
  int new_value = error_counters_.value()->num_conflicting_commits + value;
  SetAndMarkDirtyIfChanged(&error_counters_.value()->num_conflicting_commits,
      new_value,
      &error_counters_);
}

void StatusController::reset_num_conflicting_commits() {
  SetAndMarkDirtyIfChanged(&error_counters_.value()->num_conflicting_commits,
                           0, &error_counters_);
}

void StatusController::set_num_consecutive_transient_error_commits(int value) {
  SetAndMarkDirtyIfChanged(
      &error_counters_.value()->consecutive_transient_error_commits, value,
      &error_counters_);
}

void StatusController::increment_num_consecutive_transient_error_commits_by(
    int value) {
  set_num_consecutive_transient_error_commits(
      error_counters_.value()->consecutive_transient_error_commits + value);
}

void StatusController::set_num_consecutive_errors(int value) {
  SetAndMarkDirtyIfChanged(&error_counters_.value()->consecutive_errors, value,
                           &error_counters_);
}

void StatusController::set_current_sync_timestamp(int64 current_timestamp) {
  SetAndMarkDirtyIfChanged(&change_progress_.value()->current_sync_timestamp,
                           current_timestamp, &change_progress_);
}

void StatusController::set_num_server_changes_remaining(
    int64 changes_remaining) {
  SetAndMarkDirtyIfChanged(
      &change_progress_.value()->num_server_changes_remaining,
      changes_remaining, &change_progress_);
}

void StatusController::set_over_quota(bool over_quota) {
  SetAndMarkDirtyIfChanged(&syncer_status_.value()->over_quota, over_quota,
                           &syncer_status_);
}

void StatusController::set_invalid_store(bool invalid_store) {
  SetAndMarkDirtyIfChanged(&syncer_status_.value()->invalid_store,
                           invalid_store, &syncer_status_);
}

void StatusController::set_syncer_stuck(bool syncer_stuck) {
  SetAndMarkDirtyIfChanged(&syncer_status_.value()->syncer_stuck, syncer_stuck,
                           &syncer_status_);
}

void StatusController::set_syncing(bool syncing) {
  SetAndMarkDirtyIfChanged(&syncer_status_.value()->syncing, syncing,
                           &syncer_status_);
}

void StatusController::set_num_successful_bookmark_commits(int value) {
  SetAndMarkDirtyIfChanged(
      &syncer_status_.value()->num_successful_bookmark_commits,
      value, &syncer_status_);
}

void StatusController::set_num_successful_commits(int value) {
  SetAndMarkDirtyIfChanged(&syncer_status_.value()->num_successful_commits,
                           value, &syncer_status_);
}

void StatusController::set_unsynced_handles(
    const std::vector<int64>& unsynced_handles) {
  SetAndMarkDirtyIfChanged(&transient_->value()->unsynced_handles,
                           unsynced_handles, transient_.get());
}

void StatusController::increment_num_consecutive_errors() {
  set_num_consecutive_errors(error_counters_.value()->consecutive_errors + 1);
}


void StatusController::increment_num_consecutive_errors_by(int value) {
  set_num_consecutive_errors(error_counters_.value()->consecutive_errors +
                             value);
}

void StatusController::increment_num_successful_bookmark_commits() {
  set_num_successful_bookmark_commits(
      syncer_status_.value()->num_successful_bookmark_commits + 1);
}

void StatusController::increment_num_successful_commits() {
  set_num_successful_commits(
    syncer_status_.value()->num_successful_commits + 1);
}

// These setters don't affect the dirtiness of TransientState.
void StatusController::set_commit_set(const OrderedCommitSet& commit_set) {
  DCHECK(!group_restriction_in_effect_);
  commit_set_ = commit_set;
}

void StatusController::set_conflict_sets_built(bool built) {
  transient_->value()->conflict_sets_built = built;
}
void StatusController::set_conflicts_resolved(bool resolved) {
  transient_->value()->conflicts_resolved = resolved;
}
void StatusController::set_items_committed(bool items_committed) {
  transient_->value()->items_committed = items_committed;
}
void StatusController::set_timestamp_dirty(bool dirty) {
  transient_->value()->timestamp_dirty = dirty;
}

// Returns the number of updates received from the sync server.
int64 StatusController::CountUpdates() const {
  const ClientToServerResponse& updates =
      transient_->value()->updates_response;
  if (updates.has_get_updates()) {
    return updates.get_updates().entries().size();
  } else {
    return 0;
  }
}

bool StatusController::CurrentCommitIdProjectionHasIndex(size_t index) {
  OrderedCommitSet::Projection proj =
      commit_set_.GetCommitIdProjection(group_restriction_);
  return std::binary_search(proj.begin(), proj.end(), index);
}

}  // namespace sessions
}  // namespace browser_sync
