// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/test_util.h"

namespace browser_sync {
namespace sessions {
namespace test_util {

void SimulateHasMoreToSync(sessions::SyncSession* session,
                           SyncerStep begin, SyncerStep end) {
  session->mutable_status_controller()->update_conflicts_resolved(true);
  ASSERT_TRUE(session->HasMoreToSync());
}

void SimulateDownloadUpdatesFailed(sessions::SyncSession* session,
                                   SyncerStep begin, SyncerStep end) {
  // Note that a non-zero value of changes_remaining once a session has
  // completed implies that the Syncer was unable to exhaust this count during
  // the GetUpdates cycle.  This is an indication that an error occurred.
  session->mutable_status_controller()->set_num_server_changes_remaining(1);
}

void SimulateCommitFailed(sessions::SyncSession* session,
                          SyncerStep begin, SyncerStep end) {
  // Note that a non-zero number of unsynced handles once a session has
  // completed implies that the Syncer was unable to make forward progress
  // during a commit, indicating an error occurred.
  // See implementation of SyncSession::HasMoreToSync.
  std::vector<int64> handles;
  handles.push_back(1);
  session->mutable_status_controller()->set_unsynced_handles(handles);
}

void SimulateSuccess(sessions::SyncSession* session,
                     SyncerStep begin, SyncerStep end) {
  if (session->HasMoreToSync()) {
    ADD_FAILURE() << "Shouldn't have more to sync";
  }
  ASSERT_EQ(0U, session->status_controller().num_server_changes_remaining());
  ASSERT_EQ(0U, session->status_controller().unsynced_handles().size());
}

void SimulateThrottledImpl(sessions::SyncSession* session,
    const base::TimeDelta& delta) {
  session->delegate()->OnSilencedUntil(base::TimeTicks::Now() + delta);
}

void SimulatePollIntervalUpdateImpl(sessions::SyncSession* session,
    const base::TimeDelta& new_poll) {
  session->delegate()->OnReceivedLongPollIntervalUpdate(new_poll);
}

void SimulateSessionsCommitDelayUpdateImpl(sessions::SyncSession* session,
    const base::TimeDelta& new_delay) {
  session->delegate()->OnReceivedSessionsCommitDelay(new_delay);
}

}  // namespace test_util
}  // namespace sessions
}  // namespace browser_sync
