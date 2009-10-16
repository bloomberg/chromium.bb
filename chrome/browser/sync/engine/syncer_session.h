// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SyncerSession holds the entire state of a single sync cycle; GetUpdates,
// Commit, and Conflict Resolution. After said cycle, the Session may contain
// items that were unable to be processed because of errors.
//
// THIS CLASS PROVIDES NO SYNCHRONIZATION GUARANTEES.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_SESSION_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_SESSION_H_

#include <utility>
#include <vector>

#include "base/time.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/sync_cycle_state.h"
#include "chrome/browser/sync/engine/sync_process_state.h"
#include "chrome/browser/sync/engine/syncer_status.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/util/event_sys.h"
#include "chrome/browser/sync/util/sync_types.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

namespace browser_sync {

class ConflictResolver;
class ModelSafeWorker;
class ServerConnectionManager;
class SyncerStatus;
struct SyncerEvent;

class SyncerSession {
  friend class ConflictResolutionView;
  friend class SyncerStatus;
 public:
  // A utility to set the session's write transaction member, and later clear
  // it when it the utility falls out of scope.
  class ScopedSetWriteTransaction {
   public:
    ScopedSetWriteTransaction(SyncerSession* session,
                              syncable::WriteTransaction* trans)
        : session_(session) {
      session_->set_write_transaction(trans);
    }
    ~ScopedSetWriteTransaction() {
      session_->ClearWriteTransaction();
    }
   private:
    SyncerSession* session_;
    DISALLOW_COPY_AND_ASSIGN(ScopedSetWriteTransaction);
  };

  SyncerSession(SyncCycleState* cycle_state, SyncProcessState* process_state)
      : sync_process_state_(process_state),
        sync_cycle_state_(cycle_state),
        source_(sync_pb::GetUpdatesCallerInfo::UNKNOWN),
        notifications_enabled_(false) {
    DCHECK(NULL != process_state);
    DCHECK(NULL != cycle_state);
  }

  // Perhaps this should dictate the next step. (ie, don't do apply if you
  // didn't get any from download). or put it in the while loop.
  void set_update_response(const ClientToServerResponse& update_response) {
    sync_cycle_state_->set_update_response(update_response);
  }

  const ClientToServerResponse& update_response() const {
    return sync_cycle_state_->update_response();
  }

  void set_commit_response(const ClientToServerResponse& commit_response) {
    sync_cycle_state_->set_commit_response(commit_response);
  }

  const ClientToServerResponse& commit_response() const {
    return sync_cycle_state_->commit_response();
  }

  void AddVerifyResult(const VerifyResult& verify_result,
                       const sync_pb::SyncEntity& entity) {
    sync_cycle_state_->AddVerifyResult(verify_result, entity);
  }

  bool HasVerifiedUpdates() const {
    return sync_cycle_state_->HasVerifiedUpdates();
  }

  void AddAppliedUpdate(const UpdateAttemptResponse& response,
                        const syncable::Id& id) {
    sync_cycle_state_->AddAppliedUpdate(response, id);
  }

  bool HasAppliedUpdates() const {
    return sync_cycle_state_->HasAppliedUpdates();
  }

  PathString account_name() const {
    return sync_process_state_->account_name();
  }

  syncable::DirectoryManager* dirman() const {
    return sync_process_state_->dirman();
  }

  ServerConnectionManager* connection_manager() const {
    return sync_process_state_->connection_manager();
  }

  ConflictResolver* resolver() const {
    return sync_process_state_->resolver();
  }

  SyncerEventChannel* syncer_event_channel() const {
    return sync_process_state_->syncer_event_channel();
  }

  int conflicting_update_count() const {
    return sync_process_state_->conflicting_updates();
  }

  base::TimeTicks silenced_until() const {
    return sync_process_state_->silenced_until();
  }

  void set_silenced_until(base::TimeTicks silenced_until) const {
    sync_process_state_->set_silenced_until(silenced_until);
  }

  const std::vector<int64>& unsynced_handles() const {
    return sync_cycle_state_->unsynced_handles();
  }

  void set_unsynced_handles(const std::vector<int64>& unsynced_handles) {
    sync_cycle_state_->set_unsynced_handles(unsynced_handles);
  }

  int64 unsynced_count() const { return sync_cycle_state_->unsynced_count(); }

  const std::vector<syncable::Id>& commit_ids() const {
    return sync_cycle_state_->commit_ids();
  }

  void set_commit_ids(const std::vector<syncable::Id>& commit_ids) {
    sync_cycle_state_->set_commit_ids(commit_ids);
  }

  bool commit_ids_empty() const {
    return sync_cycle_state_->commit_ids_empty();
  }

  bool HadSuccessfulCommits() const {
    return sync_process_state_->successful_commits() > 0;
  }

  syncable::WriteTransaction* write_transaction() const {
    return sync_cycle_state_->write_transaction();
  }

  bool has_open_write_transaction() const {
    return sync_cycle_state_->has_open_write_transaction();
  }

  ClientToServerMessage* commit_message() const {
    return sync_cycle_state_->commit_message();
  }

  void set_commit_message(const ClientToServerMessage& message) {
    sync_cycle_state_->set_commit_message(message);
  }

  bool HasRemainingItemsToCommit() const {
    return commit_ids().size() < unsynced_handles().size();
  }

  void AddCommitConflict(const syncable::Id& the_id) {
    sync_process_state_->AddConflictingItem(the_id);
  }

  void AddBlockedItem(const syncable::Id& the_id) {
    sync_process_state_->AddBlockedItem(the_id);
  }

  void EraseCommitConflict(const syncable::Id& the_id) {
    sync_process_state_->EraseConflictingItem(the_id);
  }

  void EraseBlockedItem(const syncable::Id& the_id) {
    sync_process_state_->EraseBlockedItem(the_id);
  }

  // Returns true if at least one update application failed due to a conflict
  // during this sync cycle.
  bool HasConflictingUpdates() const {
    std::vector<AppliedUpdate>::const_iterator it;
    for (it = sync_cycle_state_->AppliedUpdatesBegin();
         it < sync_cycle_state_->AppliedUpdatesEnd();
         ++it) {
      if (it->first == CONFLICT) {
        return true;
      }
    }
    return false;
  }

  std::vector<VerifiedUpdate>::iterator VerifiedUpdatesBegin() const {
    return sync_cycle_state_->VerifiedUpdatesBegin();
  }

  std::vector<VerifiedUpdate>::iterator VerifiedUpdatesEnd() const {
    return sync_cycle_state_->VerifiedUpdatesEnd();
  }

  // Returns the number of updates received from the sync server.
  int64 CountUpdates() const {
    if (update_response().has_get_updates()) {
      return update_response().get_updates().entries().size();
    } else {
      return 0;
    }
  }

  bool got_zero_updates() const {
    return CountUpdates() == 0;
  }

  void DumpSessionInfo() const {
    LOG(INFO) << "Dumping session info";
    if (update_response().has_get_updates()) {
      LOG(INFO) << update_response().get_updates().entries().size()
                << " updates downloaded by last get_updates";
    } else {
      LOG(INFO) << "No update response found";
    }
    LOG(INFO) << sync_cycle_state_->VerifiedUpdatesSize()
              << " updates verified";
    LOG(INFO) << sync_cycle_state_->AppliedUpdatesSize() << " updates applied";
    LOG(INFO) << count_blocked_updates() << " updates blocked by open entry";
    LOG(INFO) << commit_ids().size() << " items to commit";
    LOG(INFO) << unsynced_count() << " unsynced items";
  }

  int64 count_blocked_updates() const {
    std::vector<AppliedUpdate>::const_iterator it;
    int64 count = 0;
    for (it = sync_cycle_state_->AppliedUpdatesBegin();
         it < sync_cycle_state_->AppliedUpdatesEnd();
         ++it) {
      if (it->first == BLOCKED) {
        ++count;
      }
    }
    return count;
  }

  void set_conflict_sets_built(const bool b) {
    sync_cycle_state_->set_conflict_sets_built(b);
  }

  bool conflict_sets_built() const {
    return sync_cycle_state_->conflict_sets_built();
  }

  void set_conflicts_resolved(const bool b) {
    sync_cycle_state_->set_conflicts_resolved(b);
  }

  bool conflicts_resolved() const {
    return sync_cycle_state_->conflicts_resolved();
  }

  ModelSafeWorker* model_safe_worker() const {
    return sync_process_state_->model_safe_worker();
  }

  void set_items_committed(const bool b) {
    sync_cycle_state_->set_items_committed(b);
  }

  void set_item_committed() {
    sync_cycle_state_->set_item_committed();
  }

  bool items_committed() const {
    return sync_cycle_state_->items_committed();
  }

  void set_over_quota(const bool b) {
    sync_cycle_state_->set_over_quota(b);
  }

  // Volitile reader for the source member of the syncer session object.  The
  // value is set to the SYNC_CYCLE_CONTINUATION value to signal that it has
  // been read.
  sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE TestAndSetSource() {
    sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE old_source =
        source_;
    set_source(sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION);
    return old_source;
  }

  void set_source(sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE source) {
    source_ = source;
  }

  bool notifications_enabled() const {
    return notifications_enabled_;
  }

  void set_notifications_enabled(const bool state) {
    notifications_enabled_ = state;
  }

  void set_timestamp_dirty() {
    sync_cycle_state_->set_timestamp_dirty();
  }

  bool timestamp_dirty() const {
    return sync_cycle_state_->is_timestamp_dirty();
  }

  // TODO(chron): Unit test for this method.
  // returns true iff this session contains data that should go through the
  // sync engine again.
  bool HasMoreToSync() const {
    return (HasRemainingItemsToCommit() &&
              sync_process_state_->successful_commits() > 0) ||
           conflict_sets_built() ||
           conflicts_resolved() ||
           // Or, we have conflicting updates, but we're making progress on
           // resolving them...
           !got_zero_updates() ||
           timestamp_dirty();
  }

 private:
  // The write transaction must be destructed by the caller of this function.
  // Here, we just clear the reference.
  void set_write_transaction(syncable::WriteTransaction* write_transaction) {
    sync_cycle_state_->set_write_transaction(write_transaction);
  }

  // Sets the write transaction to null, but doesn't free the memory.
  void ClearWriteTransaction() {
    sync_cycle_state_->ClearWriteTransaction();
  }

  SyncProcessState* sync_process_state_;
  SyncCycleState* sync_cycle_state_;

  // The source for initiating this syncer session.
  sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE source_;

  // True if notifications are enabled when this session was created.
  bool notifications_enabled_;

  FRIEND_TEST(SyncerTest, TestCommitListOrderingCounterexample);
  DISALLOW_COPY_AND_ASSIGN(SyncerSession);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_SESSION_H_
