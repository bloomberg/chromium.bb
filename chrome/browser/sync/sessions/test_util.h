// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utils to simulate various outcomes of a sync session.
#ifndef CHROME_BROWSER_SYNC_SESSIONS_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_SESSIONS_TEST_UTIL_H_
#pragma once

#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {
namespace sessions {
namespace test_util {

void SimulateHasMoreToSync(sessions::SyncSession* session,
                           SyncerStep begin, SyncerStep end);
void SimulateDownloadUpdatesFailed(sessions::SyncSession* session,
                                   SyncerStep begin, SyncerStep end);
void SimulateCommitFailed(sessions::SyncSession* session,
                          SyncerStep begin, SyncerStep end);
void SimulateSuccess(sessions::SyncSession* session,
                     SyncerStep begin, SyncerStep end);
void SimulateThrottledImpl(sessions::SyncSession* session,
    const base::TimeDelta& delta);
void SimulatePollIntervalUpdateImpl(sessions::SyncSession* session,
    const base::TimeDelta& new_poll);

ACTION_P(SimulateThrottled, throttle) {
  SimulateThrottledImpl(arg0, throttle);
}

ACTION_P(SimulatePollIntervalUpdate, poll) {
  SimulatePollIntervalUpdateImpl(arg0, poll);
}

}  // namespace test_util
}  // namespace sessions
}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_TEST_UTIL_H_
