// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_LIVE_SYNC_SESSIONS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_LIVE_SYNC_SESSIONS_HELPER_H_
#pragma once

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/engine/nigori_util.h"
#include "chrome/browser/sync/test/live_sync/live_sync_test.h"

class GURL;

typedef std::vector<const browser_sync::SyncedSession*> SyncedSessionVector;
typedef std::vector<SessionWindow*> SessionWindowVector;

namespace sessions_helper {

// Copies the local session windows of profile |index| to |local_windows|.
// Returns true if successful.
bool GetLocalWindows(int index, SessionWindowVector& local_windows);

// Creates and verifies the creation of a new window for profile |index| with
// one tab displaying |url|. Copies the SessionWindow associated with the new
// window to |local_windows|. Returns true if successful.
bool OpenTabAndGetLocalWindows(int index,
                               const GURL& url,
                               SessionWindowVector& local_windows);

// Checks that window count and foreign session count are 0.
bool CheckInitialState(int index);

// Returns number of open windows for a profile.
int GetNumWindows(int index);

// Returns number of foreign sessions for a profile.
int GetNumForeignSessions(int index);

// Fills the sessions vector with the model associator's foreign session data.
// Caller owns |sessions|, but not SyncedSessions objects within.
bool GetSessionData(int index, SyncedSessionVector* sessions);

// Compare session windows based on their first tab's url.
// Returns true if the virtual url of the lhs is < the rhs.
bool CompareSessionWindows(SessionWindow* lhs, SessionWindow* rhs);

// Sort session windows using our custom comparator (first tab url
// comparison).
void SortSessionWindows(SessionWindowVector& windows);

// Compares a foreign session based on the first session window.
// Returns true based on the comparison of the session windows.
bool CompareSyncedSessions(const browser_sync::SyncedSession* lhs,
                           const browser_sync::SyncedSession* rhs);

// Sort a SyncedSession vector using our custom SyncedSession comparator.
void SortSyncedSessions(SyncedSessionVector* sessions);

// Compares two tab navigations base on the parameters we sync.
// (Namely, we don't sync state or type mask)
bool NavigationEquals(const TabNavigation& expected,
                      const TabNavigation& actual);

// Verifies that two SessionWindows match.
// Returns:
//  - true if all the following match:
//    1. number of SessionWindows,
//    2. number of tabs per SessionWindow,
//    3. number of tab navigations per tab,
//    4. actual tab navigations contents
// - false otherwise.
bool WindowsMatch(const SessionWindowVector& win1,
                  const SessionWindowVector& win2);

// Retrieves the foreign sessions for a particular profile and compares them
// with a reference SessionWindow list.
// Returns true if the session windows of the foreign session matches the
// reference.
bool CheckForeignSessionsAgainst(
    int index,
    const std::vector<SessionWindowVector*>& windows);

// Open a single tab and block until the session model associator is aware
// of it. Returns true upon success, false otherwise.
bool OpenTab(int index, const GURL& url);

// Open multiple tabs and block until the session model associator is aware
// of all of them.  Returns true on success, false on failure.
bool OpenMultipleTabs(int index, const std::vector<GURL>& urls);

// Wait for a session change to propagate to the model associator.  Will not
// return until each url in |urls| has been found.
bool WaitForTabsToLoad(int index, const std::vector<GURL>& urls);

// Check if the session model associator's knows that the current open tab
// has this url.
bool ModelAssociatorHasTabWithUrl(int index, const GURL& url);

// Stores a pointer to the local session for a given profile in |session|.
// Returns true on success, false on failure.
bool GetLocalSession(int index, const browser_sync::SyncedSession** session);

}  // namespace sessions_helper

#endif  // CHROME_BROWSER_SYNC_TEST_LIVE_SYNC_SESSIONS_HELPER_H_
