// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SESSIONS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SESSIONS_HELPER_H_

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "sync/syncable/nigori_util.h"

class GURL;

namespace sessions_helper {

typedef std::vector<const browser_sync::SyncedSession*> SyncedSessionVector;
typedef browser_sync::SyncedSession::SyncedWindowMap SessionWindowMap;

// Wrapper around a SyncedWindowMap that will automatically delete the
// SessionWindow pointers it holds.
class ScopedWindowMap {
 public:
  ScopedWindowMap();
  explicit ScopedWindowMap(SessionWindowMap* windows);
  ~ScopedWindowMap();

  const SessionWindowMap* Get() const;
  SessionWindowMap* GetMutable();
  void Reset(SessionWindowMap* windows);
 private:
  SessionWindowMap windows_;
};

// Copies the local session windows of profile |index| to |local_windows|.
// Returns true if successful.
bool GetLocalWindows(int index, SessionWindowMap* local_windows);

// Creates and verifies the creation of a new window for profile |index| with
// one tab displaying |url|. Copies the SessionWindow associated with the new
// window to |local_windows|. Returns true if successful.
bool OpenTabAndGetLocalWindows(int index,
                               const GURL& url,
                               SessionWindowMap* local_windows);

// Checks that window count and foreign session count are 0.
bool CheckInitialState(int index);

// Returns number of open windows for a profile.
int GetNumWindows(int index);

// Returns number of foreign sessions for a profile.
int GetNumForeignSessions(int index);

// Fills the sessions vector with the model associator's foreign session data.
// Caller owns |sessions|, but not SyncedSessions objects within.
// Returns true if foreign sessions were found, false otherwise.
bool GetSessionData(int index, SyncedSessionVector* sessions);

// Compares a foreign session based on the first session window.
// Returns true based on the comparison of the session windows.
bool CompareSyncedSessions(const browser_sync::SyncedSession* lhs,
                           const browser_sync::SyncedSession* rhs);

// Sort a SyncedSession vector using our custom SyncedSession comparator.
void SortSyncedSessions(SyncedSessionVector* sessions);

// Compares two tab navigations base on the parameters we sync.
// (Namely, we don't sync state or type mask)
bool NavigationEquals(const sessions::SerializedNavigationEntry& expected,
                      const sessions::SerializedNavigationEntry& actual);

// Verifies that two SessionWindows match.
// Returns:
//  - true if all the following match:
//    1. number of SessionWindows,
//    2. number of tabs per SessionWindow,
//    3. number of tab navigations per tab,
//    4. actual tab navigations contents
// - false otherwise.
bool WindowsMatch(const SessionWindowMap& win1,
                  const SessionWindowMap& win2);

// Retrieves the foreign sessions for a particular profile and compares them
// with a reference SessionWindow list.
// Returns true if the session windows of the foreign session matches the
// reference.
bool CheckForeignSessionsAgainst(
    int index,
    const std::vector<ScopedWindowMap>& windows);

// Retrieves the foreign sessions for a particular profile and compares them
// to the reference windows using CheckForeignSessionsAgains. Returns true if
// they match and doesn't time out.
bool AwaitCheckForeignSessionsAgainst(
    int index, const std::vector<ScopedWindowMap>& windows);

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

// Deletes the foreign session with tag |session_tag| from the profile specified
// by |index|. This will affect all synced clients.
// Note: We pass the session_tag in by value to ensure it's not a reference
// to the session tag within the SyncedSession we plan to delete.
void DeleteForeignSession(int index, std::string session_tag);

}  // namespace sessions_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SESSIONS_HELPER_H_
