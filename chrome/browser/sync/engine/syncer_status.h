// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(sync): We eventually want to fundamentally change how we represent
// status and inform the UI about the ways in which our status has changed.
// Right now, we're just trying to keep the various command classes from having
// to worry about this class.
//
// The UI will request that we fill this struct so it can show the current sync
// state.
//
// THIS CLASS PROVIDES NO SYNCHRONIZATION GUARANTEES.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_STATUS_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_STATUS_H_

#include "base/atomicops.h"
#include "base/port.h"
#include "chrome/browser/sync/engine/sync_cycle_state.h"
#include "chrome/browser/sync/engine/sync_process_state.h"

namespace browser_sync {

class SyncerSession;

class SyncerStatus {
 public:
   SyncerStatus(SyncCycleState* cycle_state, SyncProcessState* state)
       : sync_cycle_state_(cycle_state),
         sync_process_state_(state) {}
   explicit SyncerStatus(SyncerSession* s);
   ~SyncerStatus();

  bool invalid_store() const {
    return sync_process_state_->invalid_store();
  }

  void set_invalid_store(const bool val) {
    sync_process_state_->set_invalid_store(val);
  }

  bool syncer_stuck() const {
    return sync_process_state_->syncer_stuck();
  }

  void set_syncer_stuck(const bool val) {
    sync_process_state_->set_syncer_stuck(val);
  }

  bool syncing() const {
    return sync_process_state_->syncing();
  }

  void set_syncing(const bool val) {
    sync_process_state_->set_syncing(val);
  }

  bool IsShareUsable() const {
    return sync_process_state_->IsShareUsable();
  }

  // During initial sync these two members can be used to measure sync
  // progress.
  int64 current_sync_timestamp() const {
    return sync_process_state_->current_sync_timestamp();
  }

  void set_current_sync_timestamp(const int64 val) {
    sync_process_state_->set_current_sync_timestamp(val);
  }

  int64 num_server_changes_remaining() const {
    return sync_process_state_->num_server_changes_remaining();
  }

  void set_num_server_changes_remaining(const int64 val) {
    sync_process_state_->set_num_server_changes_remaining(val);
  }

  int64 unsynced_count() const {
    return sync_cycle_state_->unsynced_count();
  }

  int conflicting_updates() const {
    return sync_process_state_->conflicting_updates();
  }

  int conflicting_commits() const {
    return sync_process_state_->conflicting_commits();
  }

  void set_conflicting_commits(const int val) {
    sync_process_state_->set_conflicting_commits(val);
  }

  int BlockedItemsSize() const {
    return sync_process_state_->BlockedItemsSize();
  }

  int stalled_updates() const {
    return sync_process_state_->BlockedItemsSize();
  }

  int error_commits() const {
    return sync_process_state_->error_commits();
  }

  void set_error_commits(const int val) {
    sync_process_state_->set_error_commits(val);
  }

  // WEIRD COUNTER manipulation functions.
  int consecutive_problem_get_updates() const {
    return sync_process_state_->consecutive_problem_get_updates();
  }

  void increment_consecutive_problem_get_updates() {
    sync_process_state_->increment_consecutive_problem_get_updates();
  }

  void zero_consecutive_problem_get_updates() {
    sync_process_state_->zero_consecutive_problem_get_updates();
  }

  int consecutive_problem_commits() const {
    return sync_process_state_->consecutive_problem_commits();
  }

  void increment_consecutive_problem_commits() {
    sync_process_state_->increment_consecutive_problem_commits();
  }

  void zero_consecutive_problem_commits() {
    sync_process_state_->zero_consecutive_problem_commits();
  }

  int consecutive_transient_error_commits() const {
    return sync_process_state_->consecutive_transient_error_commits();
  }

  void increment_consecutive_transient_error_commits_by(int value) {
    sync_process_state_->increment_consecutive_transient_error_commits_by(
        value);
  }

  void zero_consecutive_transient_error_commits() {
    sync_process_state_->zero_consecutive_transient_error_commits();
  }

  int consecutive_errors() const {
    return sync_process_state_->consecutive_errors();
  }

  void increment_consecutive_errors() {
    increment_consecutive_errors_by(1);
  }

  void increment_consecutive_errors_by(int value) {
    sync_process_state_->increment_consecutive_errors_by(value);
  }

  void zero_consecutive_errors() {
    sync_process_state_->zero_consecutive_errors();
  }

  int successful_commits() const {
    return sync_process_state_->successful_commits();
  }

  void increment_successful_commits() {
    sync_process_state_->increment_successful_commits();
  }

  void zero_successful_commits() {
    sync_process_state_->zero_successful_commits();
  }
  // End WEIRD COUNTER manipulation functions.

  bool over_quota() const { return sync_cycle_state_->over_quota(); }

  // Methods for managing error rate tracking in sync_process_state.
  void TallyNewError() {
    sync_process_state_->TallyNewError();
  }

  void TallyBigNewError() {
    sync_process_state_->TallyBigNewError();
  }

  void ForgetOldError() {
    sync_process_state_->ForgetOldError();
  }

  void CheckErrorRateTooHigh() {
    sync_process_state_->CheckErrorRateTooHigh();
  }

  void AuthFailed() { sync_process_state_->AuthFailed(); }

  void AuthSucceeded() { sync_process_state_->AuthSucceeded(); }

  // Returns true if this object has been modified since last SetClean() call.
  bool IsDirty() const {
    return sync_cycle_state_->IsDirty() || sync_process_state_->IsDirty();
  }

  // Returns true if auth status has been modified since last SetClean() call.
  bool IsAuthDirty() const { return sync_process_state_->IsAuthDirty(); }

  // Call to tell this status object that its new state has been seen.
  void SetClean() {
    sync_process_state_->SetClean();
    sync_cycle_state_->SetClean();
  }

  // Call to tell this status object that its auth state has been seen.
  void SetAuthClean() { sync_process_state_->SetAuthClean(); }

  void DumpStatusInfo() const {
    LOG(INFO) << "Dumping status info: " << (IsDirty() ? "DIRTY" : "CLEAN");

    LOG(INFO) << "invalid store = " << invalid_store();
    LOG(INFO) << "syncer_stuck = " << syncer_stuck();
    LOG(INFO) << "syncing = " << syncing();
    LOG(INFO) << "over_quota = " << over_quota();

    LOG(INFO) << "current_sync_timestamp = " << current_sync_timestamp();
    LOG(INFO) << "num_server_changes_remaining = "
              << num_server_changes_remaining();
    LOG(INFO) << "unsynced_count = " << unsynced_count();
    LOG(INFO) << "conflicting_updates = " << conflicting_updates();
    LOG(INFO) << "conflicting_commits = " << conflicting_commits();
    LOG(INFO) << "BlockedItemsSize = " << BlockedItemsSize();
    LOG(INFO) << "stalled_updates = " << stalled_updates();
    LOG(INFO) << "error_commits = " << error_commits();

    LOG(INFO) << "consecutive_problem_get_updates = "
              << consecutive_problem_get_updates();
    LOG(INFO) << "consecutive_problem_commits = "
              << consecutive_problem_commits();
    LOG(INFO) << "consecutive_transient_error_commits = "
              << consecutive_transient_error_commits();
    LOG(INFO) << "consecutive_errors = " << consecutive_errors();
    LOG(INFO) << "successful_commits = " << successful_commits();
  }

 private:
  SyncCycleState* sync_cycle_state_;
  SyncProcessState* sync_process_state_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_STATUS_H_
