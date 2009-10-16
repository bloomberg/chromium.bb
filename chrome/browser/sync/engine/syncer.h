// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/sync/engine/client_command_channel.h"
#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/directory_event.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/event_sys.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

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
class SyncerSession;
class URLFactory;
struct HttpResponse;

static const int kDefaultMaxCommitBatchSize = 25;

enum SyncerStep {
  SYNCER_BEGIN,
  DOWNLOAD_UPDATES,
  PROCESS_CLIENT_COMMAND,
  VERIFY_UPDATES,
  PROCESS_UPDATES,
  APPLY_UPDATES,
  BUILD_COMMIT_REQUEST,
  POST_COMMIT_MESSAGE,
  PROCESS_COMMIT_RESPONSE,
  BUILD_AND_PROCESS_CONFLICT_SETS,
  RESOLVE_CONFLICTS,
  APPLY_UPDATES_TO_RESOLVE_CONFLICTS,
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
  typedef void (*TestCallbackFunction)(syncable::Directory* dir);

  // The constructor may be called from a thread that is not the Syncer's
  // dedicated thread, to allow some flexibility in the setup.
  Syncer(syncable::DirectoryManager* dirman, const PathString& account_name,
      ServerConnectionManager* connection_manager,
      ModelSafeWorker* model_safe_worker);
  ~Syncer();

  // Called by other threads to tell the syncer to stop what it's doing
  // and return early from SyncShare, if possible.
  bool ExitRequested() { return early_exit_requested_; }
  void RequestEarlyExit() { early_exit_requested_ = true; }

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
  bool SyncShare();
  bool SyncShare(SyncProcessState* sync_process_state);
  bool SyncShare(SyncerStep first_step, SyncerStep last_step);

  // Limit the batch size of commit operations to a specified number of items.
  void set_max_commit_batch_size(int x) { max_commit_batch_size_ = x; }

  ConflictResolver* conflict_resolver() { return &resolver_; }

  PathString account_name() { return account_name_; }

  SyncerEventChannel* channel() const { return syncer_event_channel_.get(); }

  ShutdownChannel* shutdown_channel() const { return shutdown_channel_.get(); }

  ModelSafeWorker* model_safe_worker() { return model_safe_worker_; }

  // Syncer will take ownership of this channel and it will be destroyed along
  // with the Syncer instance.
  void set_shutdown_channel(ShutdownChannel* channel) {
    shutdown_channel_.reset(channel);
  }

  void set_command_channel(ClientCommandChannel* channel) {
    command_channel_ = channel;
  }

  // Volatile reader for the source member of the syncer session object.  The
  // value is set to the SYNC_CYCLE_CONTINUATION value to signal that it has
  // been read.
  sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE TestAndSetUpdatesSource() {
    sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE old_source =
        updates_source_;
    set_updates_source(sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION);
    return old_source;
  }

  void set_updates_source(
      sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE source) {
    updates_source_ = source;
  }

  bool notifications_enabled() const {
    return notifications_enabled_;
  }

  void set_notifications_enabled(bool state) {
    notifications_enabled_ = state;
  }

  base::TimeTicks silenced_until() const { return silenced_until_; }
  bool is_silenced() const { return !silenced_until_.is_null(); }
 private:
  void RequestNudge(int milliseconds);

  // Implements the PROCESS_CLIENT_COMMAND syncer step.
  void ProcessClientCommand(SyncerSession *session);

  void SyncShare(SyncerSession* session);
  void SyncShare(SyncerSession* session,
                 SyncerStep first_step,
                 SyncerStep last_step);

  PathString account_name_;
  bool early_exit_requested_;

  int32 max_commit_batch_size_;

  ServerConnectionManager* connection_manager_;

  ConflictResolver resolver_;
  syncable::DirectoryManager* const dirman_;

  // When we're over bandwidth quota, we don't update until past this time.
  base::TimeTicks silenced_until_;

  scoped_ptr<SyncerEventChannel> syncer_event_channel_;
  scoped_ptr<ShutdownChannel> shutdown_channel_;
  ClientCommandChannel* command_channel_;

  // A worker capable of processing work closures on a thread that is
  // guaranteed to be safe for model modifications. This is created and owned
  // by the SyncerThread that created us.
  ModelSafeWorker* model_safe_worker_;

  // The source of the last nudge.
  sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE updates_source_;

  // True only if the notification channel is authorized and open.
  bool notifications_enabled_;

  // A callback hook used in unittests to simulate changes between conflict set
  // building and conflict resolution.
  TestCallbackFunction pre_conflict_resolution_function_;

  FRIEND_TEST(SyncerTest, NewServerItemInAFolderHierarchyWeHaveDeleted3);
  FRIEND_TEST(SyncerTest, TestCommitListOrderingAndNewParent);
  FRIEND_TEST(SyncerTest, TestCommitListOrderingAndNewParentAndChild);
  FRIEND_TEST(SyncerTest, TestCommitListOrderingCounterexample);
  FRIEND_TEST(SyncerTest, TestCommitListOrderingWithNesting);
  FRIEND_TEST(SyncerTest, TestCommitListOrderingWithNewItems);
  FRIEND_TEST(SyncerTest, TestGetUnsyncedAndSimpleCommit);

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
void SplitServerInformationIntoNewEntry(syncable::WriteTransaction* trans,
                                        syncable::MutableEntry* entry);
void CopyServerFields(syncable::Entry* src, syncable::MutableEntry* dest);
void ClearServerData(syncable::MutableEntry* entry);

// Get update contents as a string. Intended for logging, and intended
// to have a smaller footprint than the protobuf's built-in pretty printer.
std::string SyncEntityDebugString(const sync_pb::SyncEntity& entry);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_H_
