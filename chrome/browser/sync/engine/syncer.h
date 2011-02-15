// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_H_
#pragma once

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_event.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/util/extensions_activity_monitor.h"
#include "chrome/common/deprecated/event_sys.h"
#include "chrome/common/deprecated/event_sys-inl.h"

namespace syncable {
class Directory;
class DirectoryManager;
class Entry;
class Id;
class MutableEntry;
class WriteTransaction;
}  // namespace syncable

namespace browser_sync {

class ModelSafeWorker;
class ServerConnectionManager;
class SyncProcessState;
class URLFactory;
struct HttpResponse;

enum SyncerStep {
  SYNCER_BEGIN,
  CLEANUP_DISABLED_TYPES,
  DOWNLOAD_UPDATES,
  PROCESS_CLIENT_COMMAND,
  VERIFY_UPDATES,
  PROCESS_UPDATES,
  STORE_TIMESTAMPS,
  APPLY_UPDATES,
  BUILD_COMMIT_REQUEST,
  POST_COMMIT_MESSAGE,
  PROCESS_COMMIT_RESPONSE,
  BUILD_AND_PROCESS_CONFLICT_SETS,
  RESOLVE_CONFLICTS,
  APPLY_UPDATES_TO_RESOLVE_CONFLICTS,
  CLEAR_PRIVATE_DATA,  // TODO(tim): Rename 'private' to 'user'.
  SYNCER_END
};

// A Syncer provides a control interface for driving the individual steps
// of the sync cycle.  Each cycle (hopefully) moves the client into closer
// synchronization with the server.  The individual steps are modeled
// as SyncerCommands, and the ordering of the steps is expressed using
// the SyncerStep enum.
//
// A Syncer instance expects to run on a dedicated thread.  Calls
// to SyncShare() may take an unbounded amount of time, as SyncerCommands
// may block on network i/o, on lock contention, or on tasks posted to
// other threads.
class Syncer {
 public:
  typedef std::vector<int64> UnsyncedMetaHandles;

  // The constructor may be called from a thread that is not the Syncer's
  // dedicated thread, to allow some flexibility in the setup.
  Syncer();
  virtual ~Syncer();

  // Called by other threads to tell the syncer to stop what it's doing
  // and return early from SyncShare, if possible.
  bool ExitRequested();
  void RequestEarlyExit();

  // TODO(tim): Deprecated.
  // Cause one sync cycle to occur.  Like a good parent, it is the caller's
  // responsibility to clean up after the syncer when it finishes a sync share
  // operation and honor server mandated throttles.
  virtual void SyncShare(sessions::SyncSession* session);

  // Like SyncShare() above, but |first_step| and |last_step| are provided to
  // perform a partial sync cycle, stopping after |last_step| is performed.
  virtual void SyncShare(sessions::SyncSession* session,
                         SyncerStep first_step,
                         SyncerStep last_step);

 private:
  // Implements the PROCESS_CLIENT_COMMAND syncer step.
  void ProcessClientCommand(sessions::SyncSession* session);

  bool early_exit_requested_;
  base::Lock early_exit_requested_lock_;

  ConflictResolver resolver_;

  // A callback hook used in unittests to simulate changes between conflict set
  // building and conflict resolution.
  Callback0::Type* pre_conflict_resolution_closure_;

  friend class SyncerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, NameClashWithResolver);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, IllegalAndLegalUpdates);
  FRIEND_TEST_ALL_PREFIXES(SusanDeletingTest,
                           NewServerItemInAFolderHierarchyWeHaveDeleted3);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestCommitListOrderingAndNewParent);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest,
                           TestCommitListOrderingAndNewParentAndChild);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestCommitListOrderingCounterexample);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestCommitListOrderingWithNesting);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestCommitListOrderingWithNewItems);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestGetUnsyncedAndSimpleCommit);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestPurgeWhileUnsynced);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestPurgeWhileUnapplied);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, UnappliedUpdateDuringCommit);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, DeletingEntryInFolder);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest,
                           LongChangelistCreatesFakeOrphanedEntries);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, QuicklyMergeDualCreatedHierarchy);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, LongChangelistWithApplicationConflict);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, DeletingEntryWithLocalEdits);
  FRIEND_TEST_ALL_PREFIXES(EntryCreatedInNewFolderTest,
                           EntryCreatedInNewFolderMidSync);

  DISALLOW_COPY_AND_ASSIGN(Syncer);
};

// Inline utility functions.

// Given iterator ranges from two collections sorted according to a common
// strict weak ordering, return true if the two ranges contain any common
// items, and false if they do not. This function is in this header so that it
// can be tested.
template <class Iterator1, class Iterator2>
bool SortedCollectionsIntersect(Iterator1 begin1, Iterator1 end1,
                                Iterator2 begin2, Iterator2 end2) {
  Iterator1 i1 = begin1;
  Iterator2 i2 = begin2;
  while (i1 != end1 && i2 != end2) {
    if (*i1 == *i2)
      return true;
    if (*i1 > *i2)
      ++i2;
    else
      ++i1;
  }
  return false;
}
// Utility function declarations.
void CopyServerFields(syncable::Entry* src, syncable::MutableEntry* dest);
void ClearServerData(syncable::MutableEntry* entry);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_H_

