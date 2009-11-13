// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that watches the syncer and attempts to resolve any conflicts that
// occur.

#ifndef CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLVER_H_
#define CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLVER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/conflict_resolution_view.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncer_status.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/util/event_sys.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

namespace syncable {
class BaseTransaction;
class Id;
class MutableEntry;
class ScopedDirLookup;
class WriteTransaction;
}  // namespace syncable

namespace browser_sync {

class ConflictResolver {
  friend class SyncerTest;
  FRIEND_TEST(SyncerTest, ConflictResolverMergeOverwritesLocalEntry);
 public:
  ConflictResolver();
  ~ConflictResolver();
  // Called by the syncer at the end of a update/commit cycle.
  // Returns true if the syncer should try to apply its updates again.
  bool ResolveConflicts(const syncable::ScopedDirLookup& dir,
                        ConflictResolutionView* view,
                        SyncerSession* session);

  // Called by ProcessServerClientNameClash. Returns true if it's merged the
  // items, false otherwise. Does not re-check preconditions covered in
  // ProcessServerClientNameClash (i.e. it assumes a name clash).
  bool AttemptItemMerge(syncable::WriteTransaction* trans,
                        syncable::MutableEntry* local_entry,
                        syncable::MutableEntry* server_entry);

 private:
  // We keep a map to record how often we've seen each conflict set. We use this
  // to screen out false positives caused by transient server or client states,
  // and to allow us to try to make smaller changes to fix situations before
  // moving onto more drastic solutions.
  typedef std::string ConflictSetCountMapKey;
  typedef std::map<ConflictSetCountMapKey, int> ConflictSetCountMap;
  typedef std::map<syncable::Id, int> SimpleConflictCountMap;

  enum ProcessSimpleConflictResult {
    NO_SYNC_PROGRESS,  // No changes to advance syncing made.
    SYNC_PROGRESS,     // Progress made.
  };

  enum ServerClientNameClashReturn {
    NO_CLASH,
    SOLUTION_DEFERRED,
    SOLVED,
    BOGUS_SET,
  };

  // Get a key for the given set. NOTE: May reorder set contents. The key is
  // currently not very efficient, but will ease debugging.
  ConflictSetCountMapKey GetSetKey(ConflictSet* conflict_set);

  void IgnoreLocalChanges(syncable::MutableEntry* entry);
  void OverwriteServerChanges(syncable::WriteTransaction* trans,
                              syncable::MutableEntry* entry);

  ProcessSimpleConflictResult ProcessSimpleConflict(
      syncable::WriteTransaction* trans,
      syncable::Id id,
      SyncerSession* session);

  bool ResolveSimpleConflicts(const syncable::ScopedDirLookup& dir,
                              ConflictResolutionView* view,
                              SyncerSession* session);

  bool ProcessConflictSet(syncable::WriteTransaction* trans,
                          ConflictSet* conflict_set,
                          int conflict_count,
                          SyncerSession* session);

  // Gives any unsynced entries in the given set new names if possible.
  bool RenameUnsyncedEntries(syncable::WriteTransaction* trans,
                             ConflictSet* conflict_set);

  ServerClientNameClashReturn ProcessServerClientNameClash(
      syncable::WriteTransaction* trans,
      syncable::MutableEntry* locally_named,
      syncable::MutableEntry* server_named,
      SyncerSession* session);
  ServerClientNameClashReturn ProcessNameClashesInSet(
      syncable::WriteTransaction* trans,
      ConflictSet* conflict_set,
      SyncerSession* session);

  // Returns true if we're stuck.
  template <typename InputIt>
  bool LogAndSignalIfConflictStuck(syncable::BaseTransaction* trans,
                                   int attempt_count,
                                   InputIt start, InputIt end,
                                   ConflictResolutionView* view);

  ConflictSetCountMap conflict_set_count_map_;
  SimpleConflictCountMap simple_conflict_count_map_;

  // Contains the ids of uncommitted items that are children of entries merged
  // in the previous cycle. This is used to speed up the merge resolution of
  // deep trees. Used to happen in store refresh.
  // TODO(chron): Can we get rid of this optimization?
  std::set<syncable::Id> children_of_merged_dirs_;

  DISALLOW_COPY_AND_ASSIGN(ConflictResolver);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLVER_H_
