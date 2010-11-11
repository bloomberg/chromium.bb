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
#include "base/lock.h"
#include "base/scoped_ptr.h"
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

static const int kDefaultMaxCommitBatchSize = 25;

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
  CLEAR_PRIVATE_DATA,
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
  explicit Syncer(sessions::SyncSessionContext* context);
  ~Syncer();

  // Called by other threads to tell the syncer to stop what it's doing
  // and return early from SyncShare, if possible.
  bool ExitRequested();
  void RequestEarlyExit();

  // SyncShare(...) variants cause one sync cycle to occur.  The return value
  // indicates whether we should sync again.  If we should not sync again,
  // it doesn't necessarily mean everything is OK; we could be throttled for
  // example.  Like a good parent, it is the caller's responsibility to clean up
  // after the syncer when it finishes a sync share operation and honor
  // server mandated throttles.
  // The zero-argument version of SyncShare is provided for unit tests.
  // When |sync_process_state| is provided, it is used as the syncer state
  // for the sync cycle.  It is treated as an input/output parameter.
  // When |first_step| and |last_step| are provided, this means to perform
  // a partial sync cycle, stopping after |last_step| is performed.
  bool SyncShare(sessions::SyncSession::Delegate* delegate);
  bool SyncShare(SyncerStep first_step, SyncerStep last_step,
                 sessions::SyncSession::Delegate* delegate);

  // Limit the batch size of commit operations to a specified number of items.
  void set_max_commit_batch_size(int x) { max_commit_batch_size_ = x; }

  // Volatile reader for the source member of the syncer session object.  The
  // value is set to the SYNC_CYCLE_CONTINUATION value to signal that it has
  // been read.
  sessions::SyncSourceInfo TestAndSetUpdatesSource() {
    sessions::SyncSourceInfo old_source = updates_source_;
    set_updates_source(sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION,
        updates_source_.second);
    return old_source;
  }

  void set_updates_source(
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
      const syncable::ModelTypeBitSet& datatypes) {
    updates_source_ = sessions::SyncSourceInfo(source, datatypes);
  }

 private:
  void RequestNudge(int milliseconds);

  // Implements the PROCESS_CLIENT_COMMAND syncer step.
  void ProcessClientCommand(sessions::SyncSession *session);

  // Resets transient state and runs from SYNCER_BEGIN to SYNCER_END.
  bool SyncShare(sessions::SyncSession* session);

  // This is the bottom-most SyncShare variant, and does not cause transient
  // state to be reset in session.
  void SyncShare(sessions::SyncSession* session,
                 SyncerStep first_step,
                 SyncerStep last_step);

  bool early_exit_requested_;
  Lock early_exit_requested_lock_;

  int32 max_commit_batch_size_;

  ConflictResolver resolver_;
  sessions::ScopedSessionContextConflictResolver resolver_scoper_;
  sessions::SyncSessionContext* context_;

  // The source of the last nudge.
  sessions::SyncSourceInfo updates_source_;

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
