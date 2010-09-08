// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_TYPES_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_TYPES_H_
#pragma once

#include <map>
#include <vector>

#include "chrome/browser/sync/util/channel.h"

namespace syncable {
class BaseTransaction;
class Id;
}

// The intent of this is to keep all shared data types and enums for the syncer
// in a single place without having dependencies between other files.
namespace browser_sync {

namespace sessions {
struct SyncSessionSnapshot;
}
class Syncer;

enum UpdateAttemptResponse {
  // Update was applied or safely ignored.
  SUCCESS,

  // Conflicts with the local data representation. This can also mean that the
  // entry doesn't currently make sense if we applied it.
  CONFLICT,
};

enum ServerUpdateProcessingResult {
  // Success. Update applied and stored in SERVER_* fields or dropped if
  // irrelevant.
  SUCCESS_PROCESSED,

  // Success. Update details stored in SERVER_* fields, but wasn't applied.
  SUCCESS_STORED,

  // Update is illegally inconsistent with earlier updates. e.g. A bookmark
  // becoming a folder.
  FAILED_INCONSISTENT,

  // Update is illegal when considered alone. e.g. broken UTF-8 in the name.
  FAILED_CORRUPT,

  // Only used by VerifyUpdate. Indicates that an update is valid. As
  // VerifyUpdate cannot return SUCCESS_STORED, we reuse the value.
  SUCCESS_VALID = SUCCESS_STORED
};

// Different results from the verify phase will yield different methods of
// processing in the ProcessUpdates phase. The SKIP result means the entry
// doesn't go to the ProcessUpdates phase.
enum VerifyResult {
  VERIFY_FAIL,
  VERIFY_SUCCESS,
  VERIFY_UNDELETE,
  VERIFY_SKIP,
  VERIFY_UNDECIDED
};

enum VerifyCommitResult {
  VERIFY_UNSYNCABLE,
  VERIFY_OK,
};

struct SyncerEvent {
  typedef SyncerEvent EventType;

  enum EventCause {
    COMMITS_SUCCEEDED,  // Count is stored in successful_commit_count.

    STATUS_CHANGED,

    // Take care not to wait in shutdown handlers for the syncer to stop as it
    // causes a race in the event system. Use SyncerShutdownEvent instead.
    SHUTDOWN_USE_WITH_CARE,

    // We're over our quota.
    OVER_QUOTA,

    // This event is how the syncer requests that it be synced.
    REQUEST_SYNC_NUDGE,

    // We have reached the SYNCER_END state in the main sync loop.
    // Check the SyncerSession for information like whether we need to continue
    // syncing (SyncerSession::HasMoreToSync).
    SYNC_CYCLE_ENDED,

    // This event is sent when the thread is paused in response to a
    // pause request.
    PAUSED,

    // This event is sent when the thread is resumed in response to a
    // resume request.
    RESUMED,

    // This event is sent when the thread is waiting for a connection
    // to be established.
    WAITING_FOR_CONNECTION,

    // This event is sent when a connection has been established and
    // the thread continues.
    CONNECTED,

    // This is sent after the Syncer (and SyncerThread) have initiated self
    // halt due to no longer being permitted to communicate with the server.
    // The listener should sever the sync / browser connections and delete sync
    // data (i.e. as if the user clicked 'Stop Syncing' in the browser.
    STOP_SYNCING_PERMANENTLY,

    // These events are sent to indicate when we know the clearing of
    // server data have failed or succeeded.
    CLEAR_SERVER_DATA_SUCCEEDED,
    CLEAR_SERVER_DATA_FAILED,

    // Sent when the main syncer loop finishes.
    SYNCER_THREAD_EXITING,
  };

  explicit SyncerEvent(EventCause cause) : what_happened(cause),
                                           snapshot(NULL),
                                           successful_commit_count(0),
                                           nudge_delay_milliseconds(0) {}

  static bool IsChannelShutdownEvent(const SyncerEvent& e) {
    return SHUTDOWN_USE_WITH_CARE == e.what_happened;
  }

  // This is used to put SyncerEvents into sorted STL structures.
  bool operator < (const SyncerEvent& r) const {
    return this->what_happened < r.what_happened;
  }

  EventCause what_happened;

  // The last session used for syncing.
  const sessions::SyncSessionSnapshot* snapshot;

  int successful_commit_count;

  // How many milliseconds later should the syncer kick in? For
  // REQUEST_SYNC_NUDGE only.
  int nudge_delay_milliseconds;
};

struct SyncerShutdownEvent {
  explicit SyncerShutdownEvent(Syncer *syncer_ptr) : syncer(syncer_ptr) {}
  Syncer* syncer;
  static bool IsChannelShutdownEvent(Syncer* syncer) {
    return true;
  }
};

typedef Channel<SyncerEvent> SyncerEventChannel;

typedef Channel<SyncerShutdownEvent> ShutdownChannel;

// This struct is passed between parts of the syncer during the processing of
// one sync loop. It lives on the stack. We don't expose the number of
// conflicts during SyncShare as the conflicts may be solved automatically
// by the conflict resolver.
typedef std::vector<syncable::Id> ConflictSet;

typedef std::map<syncable::Id, ConflictSet*> IdToConflictSetMap;

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_TYPES_H_
