// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StatusController handles all counter and status related number crunching and
// state tracking on behalf of a SyncSession.  It 'controls' the model data
// defined in session_state.h.  It can track if changes occur to certain parts
// of state so that various parts of the sync engine can avoid broadcasting
// notifications if no changes occurred.  It also separates transient state
// from long-lived SyncSession state for explicitness and to facilitate
// resetting transient state.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_STATUS_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_STATUS_CONTROLLER_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/sessions/session_state.h"

namespace browser_sync {
namespace sessions {

class StatusController {
 public:
  StatusController();

  // Returns true if some portion of the session state has changed (is dirty)
  // since it was created or was last reset.
  bool TestAndClearIsDirty();

  // Discards the current transient state components that should not carry over
  // to a subsequent sync cycle (a run between states in SyncShare), and keeps
  // everything else intact. After the call, |this| is ready for use
  // as part of a new sync cycle.
  void ResetTransientState();

  ConflictProgress const* conflict_progress() const {
    return &conflict_progress_;
  }
  ConflictProgress* mutable_conflict_progress() {
    return &conflict_progress_;
  }
  const UpdateProgress& update_progress() {
    return transient_->value()->update_progress;
  }
  UpdateProgress* mutable_update_progress() {
    return &transient_->value()->update_progress;
  }
  ClientToServerMessage* mutable_commit_message() {
    return &transient_->value()->commit_message;
  }
  const ClientToServerResponse& commit_response() const {
    return transient_->value()->commit_response;
  }
  ClientToServerResponse* mutable_commit_response() {
    return &transient_->value()->commit_response;
  }
  const ClientToServerResponse& updates_response() {
    return transient_->value()->updates_response;
  }
  ClientToServerResponse* mutable_updates_response() {
    return &transient_->value()->updates_response;
  }
  const ErrorCounters& error_counters() const {
    return error_counters_.value();
  }
  const SyncerStatus& syncer_status() const {
    return syncer_status_.value();
  }
  const ChangelogProgress& change_progress() const {
    return change_progress_.value();
  }
  const std::vector<syncable::Id>& commit_ids() const {
    return transient_->value()->commit_ids;
  }
  const std::vector<int64>& unsynced_handles() const {
    return transient_->value()->unsynced_handles;
  }
  bool conflict_sets_built() const {
    return transient_->value()->conflict_sets_built;
  }
  bool conflicts_resolved() const {
    return transient_->value()->conflicts_resolved;
  }
  bool timestamp_dirty() const {
    return transient_->value()->timestamp_dirty;
  }
  bool did_commit_items() const {
    return transient_->value()->items_committed;
  }

  // Returns the number of updates received from the sync server.
  int64 CountUpdates() const;

  bool got_zero_updates() const { return CountUpdates() == 0; }

  // A toolbelt full of methods for updating counters and flags.
  void set_num_conflicting_commits(int value);
  void set_num_consecutive_problem_get_updates(int value);
  void increment_num_consecutive_problem_get_updates();
  void set_num_consecutive_problem_commits(int value);
  void increment_num_consecutive_problem_commits();
  void set_num_consecutive_transient_error_commits(int value);
  void increment_num_consecutive_transient_error_commits_by(int value);
  void set_num_consecutive_errors(int value);
  void increment_num_consecutive_errors();
  void increment_num_consecutive_errors_by(int value);
  void set_current_sync_timestamp(int64 current_timestamp);
  void set_num_server_changes_remaining(int64 changes_remaining);
  void set_over_quota(bool over_quota);
  void set_invalid_store(bool invalid_store);
  void set_syncer_stuck(bool syncer_stuck);
  void set_syncing(bool syncing);
  void set_num_successful_commits(int value);
  void increment_num_successful_commits();
  void set_unsynced_handles(const std::vector<int64>& unsynced_handles);

  void set_commit_ids(const std::vector<syncable::Id>& commit_ids);
  void set_conflict_sets_built(bool built);
  void set_conflicts_resolved(bool resolved);
  void set_items_committed(bool items_committed);
  void set_timestamp_dirty(bool dirty);

 private:
  // Dirtyable keeps a dirty bit that can be set, cleared, and checked to
  // determine if a notification should be sent due to state change.
  // This is useful when applied to any session state object if you want to know
  // that some part of that object changed.
  template <typename T>
  class Dirtyable {
   public:
    Dirtyable() : dirty_(false) {}
    void set_dirty() { dirty_ = true; }
    bool TestAndClearIsDirty();
    T* value() { return &t_; }
    const T& value() const { return t_; }
   private:
    T t_;
    bool dirty_;
  };

  // Various pieces of state we track dirtiness of.
  Dirtyable<ChangelogProgress> change_progress_;
  Dirtyable<SyncerStatus> syncer_status_;
  Dirtyable<ErrorCounters> error_counters_;

  // The transient parts of a sync session that can be reset during the session.
  // For some parts of this state, we want to track whether changes occurred so
  // we allocate a Dirtyable version.
  scoped_ptr<Dirtyable<TransientState> > transient_;

  ConflictProgress conflict_progress_;

  DISALLOW_COPY_AND_ASSIGN(StatusController);
};

}
}

#endif  // CHROME_BROWSER_SYNC_SESSIONS_STATUS_CONTROLLER_H_
