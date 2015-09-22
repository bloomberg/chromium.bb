// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_UTIL_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_UTIL_H_

class GURL;

namespace sessions {
struct SessionTab;
struct SessionWindow;
}

namespace browser_sync {

// Checks if the given url is considered interesting enough to sync. Most urls
// are considered interesting, examples of ones that are not are invalid urls,
// files, and chrome internal pages.
bool ShouldSyncURL(const GURL& url);

// Returns if the tab has any navigation entries worth syncing or not.
bool ShouldSyncSessionTab(const sessions::SessionTab& tab);

// Returns if the window has any tabs worth syncing or not.
bool ShouldSyncSessionWindow(const sessions::SessionWindow& window);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_UTIL_H_
