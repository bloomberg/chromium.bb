// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_SESSIONS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_SESSIONS_SYNC_TEST_H_
#pragma once

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/syncable/nigori_util.h"
#include "chrome/test/live_sync/live_sync_test.h"

class GURL;
class Profile;

class LiveSessionsSyncTest : public LiveSyncTest {
 public:
  explicit LiveSessionsSyncTest(TestType test_type);
  virtual ~LiveSessionsSyncTest();

  // Used to access the browser associated with a specific sync profile.
  Browser* GetBrowser(int index);

  // Sets up the new browser windows.
  virtual bool SetupClients();

  // Encrypt sessions datatype.
  bool EnableEncryption(int index) {
    return GetClient(index)->EnableEncryptionForType(syncable::SESSIONS);
  }

  // Check if Sessions are encrypted.
  bool IsEncrypted(int index) {
    return GetClient(index)->IsTypeEncrypted(syncable::SESSIONS);
  }

  // Returns a pointer to a locally scoped copy of the local session's windows.
  std::vector<SessionWindow*>* GetLocalWindows(int index);

  // Creates and verifies the creation of a new window with one tab displaying
  // the specified GURL.
  // Returns: the SessionWindow associated with the new window.
  std::vector<SessionWindow*>* InitializeNewWindowWithTab(int index,
      const GURL& url) WARN_UNUSED_RESULT;

  // Checks that window count and foreign session count are 0.
  bool CheckInitialState(int index) WARN_UNUSED_RESULT;

  // Returns number of open windows for a profile.
  int GetNumWindows(int index);

  // Returns number of foreign sessions for a profile.
  int GetNumForeignSessions(int index);

  // Fills the sessions vector with the model associator's foreign session data.
  // Caller owns |sessions|, but not SyncedSessions objects within.
  bool GetSessionData(int index, std::vector<const SyncedSession*>* sessions)
      WARN_UNUSED_RESULT;

  // Compare session windows based on their first tab's url.
  // Returns true if the virtual url of the lhs is < the rhs.
  static bool CompareSessionWindows(SessionWindow* lhs, SessionWindow* rhs);

  // Sort session windows using our custom comparator (first tab url
  // comparison).
  void SortSessionWindows(std::vector<SessionWindow*>* windows);

  // Compares a foreign session based on the first session window.
  // Returns true based on the comparison of the session windows.
  static bool CompareSyncedSessions(
      const SyncedSession* lhs,
      const SyncedSession* rhs);

  // Sort a SyncedSession vector using our custom SyncedSession comparator.
  void SortSyncedSessions(std::vector<const SyncedSession*>* sessions);

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
  bool WindowsMatch(const std::vector<SessionWindow*> &win1,
                    const std::vector<SessionWindow*> &win2) WARN_UNUSED_RESULT;

  // Retrieves the foreign sessions for a particular profile and compares them
  // with a reference SessionWindow list.
  // Returns true if the session windows of the foreign session matches the
  // reference.
  bool CheckForeignSessionsAgainst(int index,
      const std::vector<std::vector<SessionWindow*>* >& windows)
      WARN_UNUSED_RESULT;

 protected:
  // Open a single tab and block until the session model associator is aware
  // of it. Returns true upon success, false otherwise.
  bool OpenTab(int index, const GURL& url) WARN_UNUSED_RESULT;

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
  bool GetLocalSession(int index, const SyncedSession** session);

  // Clean up our mess.
  virtual void CleanUpOnMainThread();

  // Vector of our browsers for each profile.
  std::vector<Browser*> browsers_;

  // Barrier for closing the browsers we create in UI thread.
  base::WaitableEvent done_closing_;

  // List of all copies of local sessions we get from the model associator. We
  // delete them all at destruction time so that complex tests can keep
  // comparing against old SessionWindow data.
  ScopedVector<SyncedSession> session_copies;

  DISALLOW_COPY_AND_ASSIGN(LiveSessionsSyncTest);
};

class SingleClientLiveSessionsSyncTest : public LiveSessionsSyncTest {
 public:
  SingleClientLiveSessionsSyncTest()
      : LiveSessionsSyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientLiveSessionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientLiveSessionsSyncTest);
};

class TwoClientLiveSessionsSyncTest : public LiveSessionsSyncTest {
 public:
  TwoClientLiveSessionsSyncTest() : LiveSessionsSyncTest(TWO_CLIENT) {}
  virtual ~TwoClientLiveSessionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveSessionsSyncTest);
};

class MultipleClientLiveSessionsSyncTest : public LiveSessionsSyncTest {
 public:
  MultipleClientLiveSessionsSyncTest()
      : LiveSessionsSyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientLiveSessionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientLiveSessionsSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_SESSIONS_SYNC_TEST_H_
