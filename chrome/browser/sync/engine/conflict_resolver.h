// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that watches the syncer and attempts to resolve any conflicts that
// occur.

#ifndef CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLVER_H_
#define CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLVER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/sync/engine/syncer_types.h"

namespace syncable {
class BaseTransaction;
class Id;
class MutableEntry;
class ScopedDirLookup;
class WriteTransaction;
}  // namespace syncable

namespace browser_sync {

namespace sessions {
class ConflictProgress;
class StatusController;
}  // namespace sessions

class ConflictResolver {
  friend class SyncerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncerTest,
                           ConflictResolverMergeOverwritesLocalEntry);
 public:
  ConflictResolver();
  ~ConflictResolver();
  // Called by the syncer at the end of a update/commit cycle.
  // Returns true if the syncer should try to apply its updates again.
  bool ResolveConflicts(const syncable::ScopedDirLookup& dir,
                        const sessions::ConflictProgress& progress,
                        sessions::StatusController* status);

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

  // Get a key for the given set. NOTE: May reorder set contents. The key is
  // currently not very efficient, but will ease debugging.
  ConflictSetCountMapKey GetSetKey(ConflictSet* conflict_set);

  void IgnoreLocalChanges(syncable::MutableEntry* entry);
  void OverwriteServerChanges(syncable::WriteTransaction* trans,
                              syncable::MutableEntry* entry);

  ProcessSimpleConflictResult ProcessSimpleConflict(
      syncable::WriteTransaction* trans,
      const syncable::Id& id,
      sessions::StatusController* status);

  bool ResolveSimpleConflicts(const syncable::ScopedDirLookup& dir,
                              const sessions::ConflictProgress& progress,
                              sessions::StatusController* status);

  bool ProcessConflictSet(syncable::WriteTransaction* trans,
                          ConflictSet* conflict_set,
                          int conflict_count);

  // Returns true if we're stuck.
  template <typename InputIt>
  bool LogAndSignalIfConflictStuck(syncable::BaseTransaction* trans,
                                   int attempt_count,
                                   InputIt start, InputIt end,
                                   sessions::StatusController* status);

  ConflictSetCountMap conflict_set_count_map_;
  SimpleConflictCountMap simple_conflict_count_map_;

  DISALLOW_COPY_AND_ASSIGN(ConflictResolver);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLVER_H_
