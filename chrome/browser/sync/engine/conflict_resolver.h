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

class Cryptographer;

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
  bool ResolveConflicts(syncable::WriteTransaction* trans,
                        const Cryptographer* cryptographer,
                        const sessions::ConflictProgress& progress,
                        sessions::StatusController* status);

 private:
  enum ProcessSimpleConflictResult {
    NO_SYNC_PROGRESS,  // No changes to advance syncing made.
    SYNC_PROGRESS,     // Progress made.
  };

  void IgnoreLocalChanges(syncable::MutableEntry* entry);
  void OverwriteServerChanges(syncable::WriteTransaction* trans,
                              syncable::MutableEntry* entry);

  ProcessSimpleConflictResult ProcessSimpleConflict(
      syncable::WriteTransaction* trans,
      const syncable::Id& id,
      const Cryptographer* cryptographer,
      sessions::StatusController* status);

  bool ResolveSimpleConflicts(syncable::WriteTransaction* trans,
                              const Cryptographer* cryptographer,
                              const sessions::ConflictProgress& progress,
                              sessions::StatusController* status);

  DISALLOW_COPY_AND_ASSIGN(ConflictResolver);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_CONFLICT_RESOLVER_H_
