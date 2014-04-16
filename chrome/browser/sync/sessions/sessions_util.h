// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SESSIONS_UTIL_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SESSIONS_UTIL_H_

namespace browser_sync {

class SyncedTabDelegate;
class SyncedWindowDelegate;

namespace sessions_util {

// Control which local tabs we're interested in syncing.
// Ensures that the tab has valid entries.
bool ShouldSyncTab(const SyncedTabDelegate& tab);

// Decides whether |window| is interesting for tab syncing
// purposes.
bool ShouldSyncWindow(const SyncedWindowDelegate* window);

}  // namespace sessions_util

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SESSIONS_UTIL_H_
