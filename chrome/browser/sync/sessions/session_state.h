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
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/sessions/ordered_commit_set.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"

class DictionaryValue;

namespace syncable {
class DirectoryManager;
}

namespace browser_sync {
namespace sessions {

class UpdateProgress;

// A container that contains a set of datatypes with possible string payloads.
typedef std::map<syncable::ModelType, std::string> TypePayloadMap;

// Helper utils for building TypePayloadMaps.
// Make a TypePayloadMap from all the types in a ModelTypeBitSet using a
// default payload.
TypePayloadMap MakeTypePayloadMapFromBitSet(
    const syncable::ModelTypeBitSet& types,
    const std::string& payload);

// Make a TypePayloadMap for all the enabled types in a ModelSafeRoutingInfo
// using a default payload.
TypePayloadMap MakeTypePayloadMapFromRoutingInfo(
    const ModelSafeRoutingInfo& routes,
    const std::string& payload);

// Caller takes ownership of the returned dictionary.
DictionaryValue* TypePayloadMapToValue(const TypePayloadMap& type_payloads);

// Coalesce |update| into |original|, overwriting only when |update| has
// a non-empty payload.
void CoalescePayloads(TypePayloadMap* original, const TypePayloadMap& update);

// A container for the source of a sync session. This includes the update
// source, the datatypes triggering the sync session, and possible session
// specific payloads which should be sent to the server.
struct SyncSourceInfo {
  SyncSourceInfo();
  SyncSourceInfo(
      const TypePayloadMap& t);
  SyncSourceInfo(
      const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource& u,
      const TypePayloadMap& t);
  ~SyncSourceInfo();

  // Caller takes ownership of the returned dictionary.
  DictionaryValue* ToValue() const;

  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source;
  TypePayloadMap types;
};

// Data pertaining to the status of an active Syncer object.
struct SyncerStatus {
  SyncerStatus();

  // Caller takes ownership of the returned dictionary.
  DictionaryValue* ToValue() const;

  // True when we get such an INVALID_STORE error from the server.
  bool invalid_store;
  // True iff we're stuck.
  bool syncer_stuck;
  bool syncing;
  int num_successful_commits;
  // This is needed for monitoring extensions activity.
  int num_successful_bookmark_commits;

  // Download event counters.
  int num_updates_downloaded_total;
  int num_tombstone_updates_downloaded_total;
};

// Counters for various errors that can occur repeatedly during a sync session.
struct ErrorCounters {
  ErrorCounters();

  // Caller takes ownership of the returned dictionary.
  DictionaryValue* ToValue() const;

  int num_conflicting_commits;

  // Number of commits hitting transient errors since the last successful
  // commit.
  int consecutive_transient_error_commits;

  // Incremented when get_updates fails, commit fails, and when hitting
  // transient errors. When any of these succeed, this counter is reset.
  // TODO(chron): Reduce number of weird counters we use.
  int consecutive_errors;
};

// Caller takes ownership of the returned dictionary.
DictionaryValue* DownloadProgressMarkersToValue(
    const std::string
        (&download_progress_markers)[syncable::MODEL_TYPE_COUNT]);

// An immutable snapshot of state from a SyncSession.  Convenient to use as
// part of notifications as it is inherently thread-safe.
struct SyncSessionSnapshot {
  SyncSessionSnapshot(
      const SyncerStatus& syncer_status,
      const ErrorCounters& errors,
      int64 num_server_changes_remaining,
      bool is_share_usable,
      const syncable::ModelTypeBitSet& initial_sync_ended,
      const std::string
          (&download_progress_markers)[syncable::MODEL_TYPE_COUNT],
      bool more_to_sync,
      bool is_silenced,
      int64 unsynced_count,
      int num_conflicting_updates,
      bool did_commit_items,
      const SyncSourceInfo& source);
  ~SyncSessionSnapshot();

  // Caller takes ownership of the returned dictionary.
  DictionaryValue* ToValue() const;

  const SyncerStatus syncer_status;
  const ErrorCounters errors;
  const int64 num_server_changes_remaining;
  const bool is_share_usable;
  const syncable::ModelTypeBitSet initial_sync_ended;
  const std::string download_progress_markers[syncable::MODEL_TYPE_COUNT];
  const bool has_more_to_sync;
  const bool is_silenced;
  const int64 unsynced_count;
  const int num_conflicting_updates;
  const bool did_commit_items;
  const SyncSourceInfo source;
};

// Tracks progress of conflicts and their resolution using conflict sets.
class ConflictProgress {
 public:
  explicit ConflictProgress(bool* dirty_flag);
  ~ConflictProgress();
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

 private:
  // TODO(sync): move away from sets if it makes more sense.
  std::set<syncable::Id> conflicting_item_ids_;
  std::map<syncable::Id, ConflictSet*> id_to_conflict_set_;
  std::set<ConflictSet*> conflict_sets_;

  // Whether a conflicting item was added or removed since
  // the last call to reset_progress_changed(), if any. In practice this
  // points to StatusController::is_dirty_.
  bool* dirty_;
};

typedef std::pair<VerifyResult, sync_pb::SyncEntity> VerifiedUpdate;
typedef std::pair<UpdateAttemptResponse, syncable::Id> AppliedUpdate;

// Tracks update application and verification.
class UpdateProgress {
 public:
  UpdateProgress();
  ~UpdateProgress();

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
  // Container for updates that passed verification.
  std::vector<VerifiedUpdate> verified_updates_;

  // Stores the result of the various ApplyUpdate attempts we've made.
  // May contain duplicate entries.
  std::vector<AppliedUpdate> applied_updates_;
};

struct SyncCycleControlParameters {
  SyncCycleControlParameters() : conflict_sets_built(false),
                                 conflicts_resolved(false),
                                 items_committed(false) {}
  // Set to true by BuildAndProcessConflictSetsCommand if the RESOLVE_CONFLICTS
  // step is needed.
  bool conflict_sets_built;

  // Set to true by ResolveConflictsCommand if any forward progress was made.
  bool conflicts_resolved;

  // Set to true by PostCommitMessageCommand if any commits were successful.
  bool items_committed;
};

// DirtyOnWrite wraps a value such that any write operation will update a
// specified dirty bit, which can be used to determine if a notification should
// be sent due to state change.
template <typename T>
class DirtyOnWrite {
 public:
  explicit DirtyOnWrite(bool* dirty) : dirty_(dirty) {}
  DirtyOnWrite(bool* dirty, const T& t) : t_(t), dirty_(dirty) {}
  T* mutate() {
    *dirty_ = true;
    return &t_;
  }
  const T& value() const { return t_; }
 private:
  T t_;
  bool* dirty_;
};

// The next 3 structures declare how all the state involved in running a sync
// cycle is divided between global scope (applies to all model types),
// ModelSafeGroup scope (applies to all data types in a group), and single
// model type scope.  Within this breakdown, each struct declares which bits
// of state are dirty-on-write and should incur dirty bit updates if changed.

// Grouping of all state that applies to all model types.  Note that some
// components of the global grouping can internally implement finer grained
// scope control (such as OrderedCommitSet), but the top level entity is still
// a singleton with respect to model types.
struct AllModelTypeState {
  explicit AllModelTypeState(bool* dirty_flag);
  ~AllModelTypeState();

  // Commits for all model types are bundled together into a single message.
  ClientToServerMessage commit_message;
  ClientToServerResponse commit_response;
  // We GetUpdates for some combination of types at once.
  // requested_update_types stores the set of types which were requested.
  syncable::ModelTypeBitSet updates_request_types;
  ClientToServerResponse updates_response;
  // Used to build the shared commit message.
  DirtyOnWrite<std::vector<int64> > unsynced_handles;
  DirtyOnWrite<SyncerStatus> syncer_status;
  DirtyOnWrite<ErrorCounters> error_counters;
  SyncCycleControlParameters control_params;
  DirtyOnWrite<int64> num_server_changes_remaining;
  OrderedCommitSet commit_set;
};

// Grouping of all state that applies to a single ModelSafeGroup.
struct PerModelSafeGroupState {
  explicit PerModelSafeGroupState(bool* dirty_flag);
  ~PerModelSafeGroupState();

  UpdateProgress update_progress;
  ConflictProgress conflict_progress;
};

}  // namespace sessions
}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SESSION_STATE_H_
