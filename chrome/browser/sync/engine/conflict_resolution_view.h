// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Conflict resolution view is intended to provide a restricted view of the
// sync cycle state for the conflict resolver. Since the resolver doesn't get
// to see all of the SyncProcess, we can allow it to operate on a subsection of
// the data.

#ifndef CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLUTION_VIEW_H_
#define CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLUTION_VIEW_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer_types.h"

namespace syncable {
class Id;
}

namespace browser_sync {

class SyncCycleState;
class SyncProcessState;
class SyncerSession;

class ConflictResolutionView {
  // THIS CLASS PROVIDES NO SYNCHRONIZATION GUARANTEES.
 public:

  explicit ConflictResolutionView(SyncProcessState* state)
      : process_state_(state) {
  }

  explicit ConflictResolutionView(SyncerSession* session);

  ~ConflictResolutionView() {}

  int conflicting_updates() const;

  // TODO(sync) can successful commit go in session?
  int successful_commits() const;

  void increment_successful_commits();

  void zero_successful_commits();

  int conflicting_commits() const;

  void set_conflicting_commits(const int val);

  int num_sync_cycles() const;

  void increment_num_sync_cycles();

  void zero_num_sync_cycles();

  // True iff we're stuck. Something has gone wrong with the syncer.
  bool syncer_stuck() const;

  void set_syncer_stuck(const bool val);

  int64 current_sync_timestamp() const;

  int64 num_server_changes_remaining() const;

  IdToConflictSetMap::const_iterator IdToConflictSetFind(
      const syncable::Id& the_id) const;

  IdToConflictSetMap::const_iterator IdToConflictSetBegin() const;

  IdToConflictSetMap::const_iterator IdToConflictSetEnd() const;

  IdToConflictSetMap::size_type IdToConflictSetSize() const;

  const ConflictSet* IdToConflictSetGet(const syncable::Id& the_id);

  std::set<ConflictSet*>::const_iterator ConflictSetsBegin() const;

  std::set<ConflictSet*>::const_iterator ConflictSetsEnd() const;

  std::set<ConflictSet*>::size_type ConflictSetsSize() const;

  void MergeSets(const syncable::Id& set1, const syncable::Id& set2);

  void CleanupSets();

  bool HasCommitConflicts() const;

  bool HasBlockedItems() const;

  int CommitConflictsSize() const;

  int BlockedItemsSize() const;

  void AddCommitConflict(const syncable::Id& the_id);

  void AddBlockedItem(const syncable::Id& the_id);

  void EraseCommitConflict(std::set<syncable::Id>::iterator it);

  void EraseBlockedItem(std::set<syncable::Id>::iterator it);

  std::set<syncable::Id>::iterator CommitConflictsBegin() const;

  std::set<syncable::Id>::iterator BlockedItemsBegin() const;

  std::set<syncable::Id>::iterator CommitConflictsEnd() const;

  std::set<syncable::Id>::iterator BlockedItemsEnd() const;

 private:
  SyncProcessState* process_state_;

  DISALLOW_COPY_AND_ASSIGN(ConflictResolutionView);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLUTION_VIEW_H_
