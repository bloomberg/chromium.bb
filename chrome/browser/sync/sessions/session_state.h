// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The 'sessions' namespace comprises all the pieces of state that are
// combined to form a SyncSession instance. In that way, it can be thought of
// as an extension of the SyncSession type itself. Session scoping gives
// context to things like "conflict progress", "update progress", etc, and the
// separation this file provides allows clients to only include the parts they
// need rather than the entire session stack.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SESSION_STATE_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SESSION_STATE_H_

#include <set>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncproto.h"

namespace syncable {
class DirectoryManager;
}

namespace browser_sync {
namespace sessions {

class UpdateProgress;

// Data describing the progress made relative to the changelog on the server.
struct ChangelogProgress {
  ChangelogProgress() : current_sync_timestamp(0),
                        num_server_changes_remaining(0) {}
  int64 current_sync_timestamp;
  int64 num_server_changes_remaining;
};

// Data pertaining to the status of an active Syncer object.
struct SyncerStatus {
  SyncerStatus()
      : over_quota(false), invalid_store(false), syncer_stuck(false),
        syncing(false), num_successful_commits(0) {}
  bool over_quota;
  // True when we get such an INVALID_STORE error from the server.
  bool invalid_store;
  // True iff we're stuck.
  bool syncer_stuck;
  bool syncing;
  int num_successful_commits;
};

// Counters for various errors that can occur repeatedly during a sync session.
struct ErrorCounters {
  ErrorCounters() : num_conflicting_commits(0),
                    consecutive_problem_get_updates(0),
                    consecutive_problem_commits(0),
                    consecutive_transient_error_commits(0),
                    consecutive_errors(0) {}
  int num_conflicting_commits;
  // Is reset when we get any updates (not on pings) and increments whenever
  // the request fails.
  int consecutive_problem_get_updates;

  // Consecutive_problem_commits_ resets whenever we commit any number of items
  // and increments whenever all commits fail for any reason.
  int consecutive_problem_commits;

  // Number of commits hitting transient errors since the last successful
  // commit.
  int consecutive_transient_error_commits;

  // Incremented when get_updates fails, commit fails, and when hitting
  // transient errors. When any of these succeed, this counter is reset.
  // TODO(chron): Reduce number of weird counters we use.
  int consecutive_errors;
};

// An immutable snapshot of state from a SyncSession.  Convenient to use as
// part of notifications as it is inherently thread-safe.
struct SyncSessionSnapshot {
  SyncSessionSnapshot(const SyncerStatus& syncer_status,
      const ErrorCounters& errors,
      const ChangelogProgress& changelog_progress, bool is_share_usable,
      bool more_to_sync, bool is_silenced, int64 unsynced_count,
      int num_conflicting_updates, bool did_commit_items)
      : syncer_status(syncer_status),
        errors(errors),
        changelog_progress(changelog_progress),
        is_share_usable(is_share_usable),
        has_more_to_sync(more_to_sync),
        is_silenced(is_silenced),
        unsynced_count(unsynced_count),
        num_conflicting_updates(num_conflicting_updates),
        did_commit_items(did_commit_items) {}
  const SyncerStatus syncer_status;
  const ErrorCounters errors;
  const ChangelogProgress changelog_progress;
  const bool is_share_usable;
  const bool has_more_to_sync;
  const bool is_silenced;
  const int64 unsynced_count;
  const int num_conflicting_updates;
  const bool did_commit_items;
};

// Tracks progress of conflicts and their resolution using conflict sets.
class ConflictProgress {
 public:
  ConflictProgress() : progress_changed_(false) {}
  ~ConflictProgress() { CleanupSets(); }
  // Various iterators, size, and retrieval functions for conflict sets.
  IdToConflictSetMap::const_iterator IdToConflictSetBegin() const;
  IdToConflictSetMap::const_iterator IdToConflictSetEnd() const;
  IdToConflictSetMap::size_type IdToConflictSetSize() const;
  IdToConflictSetMap::const_iterator IdToConflictSetFind(
      const syncable::Id& the_id) const;
  const ConflictSet* IdToConflictSetGet(const syncable::Id& the_id);
  std::set<ConflictSet*>::const_iterator ConflictSetsBegin() const;
  std::set<ConflictSet*>::const_iterator ConflictSetsEnd() const;
  std::set<ConflictSet*>::size_type ConflictSetsSize() const;

  // Various mutators for tracking commit conflicts.
  void AddConflictingItemById(const syncable::Id& the_id);
  void EraseConflictingItemById(const syncable::Id& the_id);
  int ConflictingItemsSize() const { return conflicting_item_ids_.size(); }
  std::set<syncable::Id>::iterator ConflictingItemsBegin();
  std::set<syncable::Id>::const_iterator ConflictingItemsBeginConst() const;
  std::set<syncable::Id>::const_iterator ConflictingItemsEnd() const;

  void MergeSets(const syncable::Id& set1, const syncable::Id& set2);
  void CleanupSets();

  bool progress_changed() const { return progress_changed_; }
  void reset_progress_changed() { progress_changed_ = false; }
 private:
  // TODO(sync): move away from sets if it makes more sense.
  std::set<syncable::Id> conflicting_item_ids_;
  std::map<syncable::Id, ConflictSet*> id_to_conflict_set_;
  std::set<ConflictSet*> conflict_sets_;

  // Whether a conflicting item was added or removed since
  // the last call to reset_progress_changed(), if any.
  bool progress_changed_;
};

typedef std::pair<VerifyResult, sync_pb::SyncEntity> VerifiedUpdate;
typedef std::pair<UpdateAttemptResponse, syncable::Id> AppliedUpdate;

// Tracks update application and verification.
class UpdateProgress {
 public:
  void AddVerifyResult(const VerifyResult& verify_result,
                       const sync_pb::SyncEntity& entity);

  // Log a successful or failing update attempt.
  void AddAppliedUpdate(const UpdateAttemptResponse& response,
                        const syncable::Id& id);

  // Various iterators.
  std::vector<AppliedUpdate>::iterator AppliedUpdatesBegin();
  std::vector<VerifiedUpdate>::const_iterator VerifiedUpdatesBegin() const;
  std::vector<AppliedUpdate>::const_iterator AppliedUpdatesEnd() const;
  std::vector<VerifiedUpdate>::const_iterator VerifiedUpdatesEnd() const;

  // Returns the number of update application attempts.  This includes both
  // failures and successes.
  int AppliedUpdatesSize() const { return applied_updates_.size(); }
  int VerifiedUpdatesSize() const { return verified_updates_.size(); }
  bool HasVerifiedUpdates() const { return !verified_updates_.empty(); }
  bool HasAppliedUpdates() const { return !applied_updates_.empty(); }

  // Count the number of successful update applications that have happend this
  // cycle. Note that if an item is successfully applied twice, it will be
  // double counted here.
  int SuccessfullyAppliedUpdateCount() const;

  // Returns true if at least one update application failed due to a conflict
  // during this sync cycle.
  bool HasConflictingUpdates() const;

 private:
  // Some container for updates that failed verification.
  std::vector<VerifiedUpdate> verified_updates_;

  // Stores the result of the various ApplyUpdate attempts we've made.
  // May contain duplicate entries.
  std::vector<AppliedUpdate> applied_updates_;
};

// Transient state is a physical grouping of session state that can be reset
// while that session is in flight.  This is useful when multiple
// SyncShare operations take place during a session.
struct TransientState {
  TransientState()
    : conflict_sets_built(false),
      conflicts_resolved(false),
      items_committed(false),
      timestamp_dirty(false) {}
  UpdateProgress update_progress;
  ClientToServerMessage commit_message;
  ClientToServerResponse commit_response;
  ClientToServerResponse updates_response;
  std::vector<int64> unsynced_handles;
  std::vector<syncable::Id> commit_ids;
  bool conflict_sets_built;
  bool conflicts_resolved;
  bool items_committed;
  bool timestamp_dirty;
};

}  // namespace sessions
}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SESSION_STATE_H_
