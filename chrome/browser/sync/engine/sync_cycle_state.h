// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The sync process consists of a sequence of sync cycles, each of which
// (hopefully) moves the client into closer synchronization with the server.
// This class holds state that is pertinent to a single sync cycle.
//
// THIS CLASS PROVIDES NO SYNCHRONIZATION GUARANTEES.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNC_CYCLE_STATE_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNC_CYCLE_STATE_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/util/event_sys.h"

namespace syncable {
class WriteTransaction;
class Id;
}  // namespace syncable

namespace browser_sync {

typedef std::pair<VerifyResult, sync_pb::SyncEntity> VerifiedUpdate;
typedef std::pair<UpdateAttemptResponse, syncable::Id> AppliedUpdate;

// This is the type declaration for the eventsys channel that the syncer uses
// to send events to other system components.
struct SyncerEvent;

// SyncCycleState holds the entire state of a single sync cycle;
// GetUpdates, Commit, and Conflict Resolution. After said cycle, the
// State may contain items that were unable to be processed because of
// errors.
class SyncCycleState {
 public:
  SyncCycleState()
      : write_transaction_(NULL),
        conflict_sets_built_(false),
        conflicts_resolved_(false),
        items_committed_(false),
        over_quota_(false),
        timestamp_dirty_(false),
        dirty_(true) {}

  void set_update_response(const ClientToServerResponse& update_response) {
    update_response_.CopyFrom(update_response);
  }

  const ClientToServerResponse& update_response() const {
    return update_response_;
  }

  void set_commit_response(const ClientToServerResponse& commit_response) {
    commit_response_.CopyFrom(commit_response);
  }

  const ClientToServerResponse& commit_response() const {
    return commit_response_;
  }

  void AddVerifyResult(const VerifyResult& verify_result,
                       const sync_pb::SyncEntity& entity) {
    verified_updates_.push_back(std::make_pair(verify_result, entity));
  }

  bool HasVerifiedUpdates() const {
    return !verified_updates_.empty();
  }

  // Log a successful or failing update attempt.
  void AddAppliedUpdate(const UpdateAttemptResponse& response,
                        const syncable::Id& id) {
    applied_updates_.push_back(std::make_pair(response, id));
  }

  bool HasAppliedUpdates() const {
    return !applied_updates_.empty();
  }

  std::vector<AppliedUpdate>::iterator AppliedUpdatesBegin() {
    return applied_updates_.begin();
  }

  std::vector<VerifiedUpdate>::iterator VerifiedUpdatesBegin() {
    return verified_updates_.begin();
  }

  std::vector<AppliedUpdate>::iterator AppliedUpdatesEnd() {
    return applied_updates_.end();
  }

  std::vector<VerifiedUpdate>::iterator VerifiedUpdatesEnd() {
    return verified_updates_.end();
  }

  // Returns the number of update application attempts.  This includes both
  // failures and successes.
  int AppliedUpdatesSize() const {
    return applied_updates_.size();
  }

  // Count the number of successful update applications that have happend this
  // cycle. Note that if an item is successfully applied twice, it will be
  // double counted here.
  int SuccessfullyAppliedUpdateCount() const {
    int count = 0;
    for (std::vector<AppliedUpdate>::const_iterator it =
             applied_updates_.begin();
         it != applied_updates_.end();
         ++it) {
      if (it->first == SUCCESS)
        count++;
    }
    return count;
  }

  int VerifiedUpdatesSize() const {
    return verified_updates_.size();
  }

  const std::vector<int64>& unsynced_handles() const {
    return unsynced_handles_;
  }

  void set_unsynced_handles(const std::vector<int64>& unsynced_handles) {
    UpdateDirty(unsynced_handles != unsynced_handles_);
    unsynced_handles_ = unsynced_handles;
  }

  int64 unsynced_count() const { return unsynced_handles_.size(); }

  const std::vector<syncable::Id>& commit_ids() const { return commit_ids_; }

  void set_commit_ids(const std::vector<syncable::Id>& commit_ids) {
    commit_ids_ = commit_ids;
  }

  bool commit_ids_empty() const { return commit_ids_.empty(); }

  // The write transaction must be deleted by the caller of this function.
  void set_write_transaction(syncable::WriteTransaction* write_transaction) {
    DCHECK(!write_transaction_) << "Forgot to clear the write transaction.";
    write_transaction_ = write_transaction;
  }

  syncable::WriteTransaction* write_transaction() const {
    return write_transaction_;
  }

  bool has_open_write_transaction() { return write_transaction_ != NULL; }

  // Sets the write transaction to null, but doesn't free the memory.
  void ClearWriteTransaction() { write_transaction_ = NULL; }

  ClientToServerMessage* commit_message() { return &commit_message_; }

  void set_commit_message(ClientToServerMessage message) {
    commit_message_ = message;
  }

  void set_conflict_sets_built(bool b) {
    conflict_sets_built_ = b;
  }

  bool conflict_sets_built() const {
    return conflict_sets_built_;
  }

  void set_conflicts_resolved(bool b) {
    conflicts_resolved_ = b;
  }

  bool conflicts_resolved() const {
    return conflicts_resolved_;
  }

  void set_over_quota(bool b) {
    UpdateDirty(b != over_quota_);
    over_quota_ = b;
  }

  bool over_quota() const {
    return over_quota_;
  }

  void set_item_committed() { items_committed_ |= true; }

  bool items_committed() const { return items_committed_; }

  // Returns true if this object has been modified since last SetClean() call.
  bool IsDirty() const { return dirty_; }

  // Call to tell this status object that its new state has been seen.
  void SetClean() { dirty_ = false; }

  // Indicate that we've made a change to directory timestamp.
  void set_timestamp_dirty() {
    timestamp_dirty_ = true;
  }

  bool is_timestamp_dirty() const {
    return timestamp_dirty_;
  }

 private:
  void UpdateDirty(bool new_info) { dirty_ |= new_info; }

  // Download updates supplies:
  ClientToServerResponse update_response_;
  ClientToServerResponse commit_response_;
  ClientToServerMessage commit_message_;

  syncable::WriteTransaction* write_transaction_;
  std::vector<int64> unsynced_handles_;
  std::vector<syncable::Id> commit_ids_;

  // At a certain point during the sync process we'll want to build the
  // conflict sets. This variable tracks whether or not that has happened.
  bool conflict_sets_built_;
  bool conflicts_resolved_;
  bool items_committed_;
  bool over_quota_;

  // If we've set the timestamp to a new value during this cycle.
  bool timestamp_dirty_;

  bool dirty_;

  // Some container for updates that failed verification.
  std::vector<VerifiedUpdate> verified_updates_;

  // Stores the result of the various ApplyUpdate attempts we've made.
  // May contain duplicate entries.
  std::vector<AppliedUpdate> applied_updates_;

  DISALLOW_COPY_AND_ASSIGN(SyncCycleState);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNC_CYCLE_STATE_H_
