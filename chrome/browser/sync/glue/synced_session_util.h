// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_UTIL_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_UTIL_H_

namespace sessions {
struct SessionWindow;
}

namespace browser_sync {

// Checks whether the window has tabs to sync. If no tabs to sync, it returns
// true, false otherwise.
bool SessionWindowHasNoTabsToSync(const sessions::SessionWindow& window);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_UTIL_H_
