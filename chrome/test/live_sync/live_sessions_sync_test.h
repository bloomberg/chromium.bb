// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_SESSIONS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_SESSIONS_SYNC_TEST_H_
#pragma once

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "base/scoped_vector.h"
#include "base/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sessions/base_session_service.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/test/live_sync/live_sync_test.h"

class GURL;
class Profile;
class TabContents;

// Helper for accessing session service internals.
class TestSessionService
    : public SessionServiceTestHelper,
      public base::RefCountedThreadSafe<TestSessionService> {
 public:
  TestSessionService();
  TestSessionService(SessionService* service, Profile* profile);

  void SetUp();

  // Trigger saving the current session commands to file.
  void Save();

  // Synchronously reads the contents of the current session.
  std::vector<SessionWindow*>* ReadWindows();

  // Makes the actual call to session service.
  // Lives on the file thread.
  void DoReadWindows();

  // Internal method used in the callback to obtain the current session.
  // Lives on and called from backend thread (file_thread).
  // We don't own windows so need to make a deep copy.
  // In this case, we only copy those values compared against in WindowsMatch
  // (number of windows, number of tabs, and navigations within tabs).
  void OnGotSession(int handle, std::vector<SessionWindow*>* windows);

 private:
  virtual ~TestSessionService();

  friend class base::RefCountedThreadSafe<TestSessionService>;

  // Current session buffer.
  std::vector<SessionWindow*>* windows_;

  // List of all foreign sessions we created in ReadWindows. We delete them all
  // at destruction time so that complex tests can keep comparing against old
  // SessionWindow data. Note that since we're constantly creating new foreign
  // sessions, we don't have to worry about duplicates.
  ScopedVector<ForeignSession> foreign_sessions_;

  // Barrier for saving session.
  base::WaitableEvent done_saving_;

  // Barrier for getting current windows in ReadWindows.
  base::WaitableEvent got_windows_;

  // Consumer used to obtain the current session.
  CancelableRequestConsumer consumer_;

  // Sync profile associated with this session service.
  Profile* profile_;

  SessionID window_id_;
  const gfx::Rect window_bounds_;
};

class LiveSessionsSyncTest : public LiveSyncTest {
 public:
  explicit LiveSessionsSyncTest(TestType test_type);
  virtual ~LiveSessionsSyncTest();

  // Used to access the session service associated with a specific sync profile.
  SessionService* GetSessionService(int index);

  // Used to access the session service test helper associated with a specific
  // sync profile.
  TestSessionService* GetHelper(int index);

  // Used to access the browser associated with a specific sync profile.
  Browser* GetBrowser(int index);

  // Sets up the TestSessionService helper and the new browser windows.
  virtual bool SetupClients();

  // Open a single tab and return the TabContents. TabContents must be checked
  // to ensure the tab opened successsfully.
  TabContents* OpenTab(int index, GURL url) WARN_UNUSED_RESULT;

  // Creates and verifies the creation of a new window with one tab displaying
  // the specified GURL.
  // Returns: the SessionWindow associated with the new window.
  std::vector<SessionWindow*>* InitializeNewWindowWithTab(int index, GURL url)
      WARN_UNUSED_RESULT;

  // Checks that window count and foreign session count are 0.
  bool CheckInitialState(int index) WARN_UNUSED_RESULT;

  // Returns number of open windows for a profile.
  int GetNumWindows(int index);

  // Returns number of foreign sessions for a profile.
  int GetNumForeignSessions(int index);

  // Fills the sessions vector with the model associator's foreign session data.
  // Caller owns |sessions|, but not ForeignSession objects within.
  bool GetSessionData(int index, std::vector<const ForeignSession*>* sessions)
      WARN_UNUSED_RESULT;

  // Compare session windows based on their first tab's url.
  // Returns true if the virtual url of the lhs is < the rhs.
  static bool CompareSessionWindows(SessionWindow* lhs, SessionWindow* rhs);

  // Sort session windows using our custom comparator (first tab url
  // comparison).
  void SortSessionWindows(std::vector<SessionWindow*>* windows);

  // Compares a foreign session based on the first session window.
  // Returns true based on the comparison of the session windows.
  static bool CompareForeignSessions(
      const ForeignSession* lhs,
      const ForeignSession* rhs);

  // Sort a foreign session vector using our custom foreign session comparator.
  void SortForeignSessions(std::vector<const ForeignSession*>* sessions);

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
  // Clean up our mess.
  virtual void CleanUpOnMainThread();

  // Vector of our TestSessionService helpers.
  ScopedVector<scoped_refptr<TestSessionService> > test_session_services_;

  // Vector of our browsers for each profile.
  std::vector<Browser*> browsers_;

  // Barrier for closing the browsers we create in UI thread.
  base::WaitableEvent done_closing_;

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
