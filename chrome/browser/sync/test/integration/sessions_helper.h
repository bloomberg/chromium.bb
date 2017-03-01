// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SESSIONS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SESSIONS_HELPER_H_

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sessions/core/session_types.h"
#include "components/sync/syncable/nigori_util.h"
#include "components/sync_sessions/synced_session.h"

class GURL;

namespace sessions_helper {

using SyncedSessionVector = std::vector<const sync_sessions::SyncedSession*>;
using SessionWindowMap = std::map<SessionID::id_type, sessions::SessionWindow*>;
using ScopedWindowMap =
    std::map<SessionID::id_type, std::unique_ptr<sessions::SessionWindow>>;

// Copies the local session windows of profile |index| to |local_windows|.
// Returns true if successful.
bool GetLocalWindows(int index, ScopedWindowMap* local_windows);

// Creates and verifies the creation of a new window for profile |index| with
// one tab displaying |url|. Copies the SessionWindow associated with the new
// window to |local_windows|. Returns true if successful. This call results in
// multiple sessions changes, and performs synchronous blocking. It is rare, but
// possible, that multiple sync cycle commits occur as a result of this call.
// Test cases should be written to handle this possibility, otherwise they may
// flake.
bool OpenTabAndGetLocalWindows(int index,
                               const GURL& url,
                               ScopedWindowMap* local_windows);

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
bool CompareSyncedSessions(const sync_sessions::SyncedSession* lhs,
                           const sync_sessions::SyncedSession* rhs);

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
bool WindowsMatch(const ScopedWindowMap& win1, const ScopedWindowMap& win2);
bool WindowsMatch(const SessionWindowMap& win1, const ScopedWindowMap& win2);

// Retrieves the foreign sessions for a particular profile and compares them
// with a reference SessionWindow list.
// Returns true if the session windows of the foreign session matches the
// reference.
bool CheckForeignSessionsAgainst(
    int index,
    const std::vector<ScopedWindowMap>& windows);

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
bool GetLocalSession(int index, const sync_sessions::SyncedSession** session);

// Deletes the foreign session with tag |session_tag| from the profile specified
// by |index|. This will affect all synced clients.
// Note: We pass the session_tag in by value to ensure it's not a reference
// to the session tag within the SyncedSession we plan to delete.
void DeleteForeignSession(int index, std::string session_tag);

}  // namespace sessions_helper

// Checker to block until the foreign sessions for a particular profile matches
// the reference windows.
class ForeignSessionsMatchChecker : public MultiClientStatusChangeChecker {
 public:
  ForeignSessionsMatchChecker(
      int index,
      const std::vector<sessions_helper::ScopedWindowMap>& windows);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied() override;
  std::string GetDebugMessage() const override;

 private:
  int index_;
  const std::vector<sessions_helper::ScopedWindowMap>& windows_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SESSIONS_HELPER_H_
