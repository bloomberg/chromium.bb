// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "base/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/sessions/base_session_service.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents_wrapper.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/window_open_disposition.h"

// Helper for accessing session service internals.
class TestSessionService
    : public SessionServiceTestHelper,
      public base::RefCountedThreadSafe<TestSessionService> {
 public:
  TestSessionService()
      : SessionServiceTestHelper(),
        done_saving_(false, false),
        got_windows_(false, false),
        profile_(NULL),
        window_bounds_(0, 1, 2, 3) {}
  TestSessionService(SessionService * service,
                              Profile* profile)
      : SessionServiceTestHelper(service),
        done_saving_(false, false),
        got_windows_(false, false),
        profile_(profile),
        window_bounds_(0, 1, 2, 3) {}

  void SetUp() {
    ASSERT_TRUE(service()) << "SetUp() called without setting SessionService";
    ASSERT_TRUE(profile_);
    service()->SetWindowType(window_id_, Browser::TYPE_NORMAL);
    service()->SetWindowBounds(window_id_, window_bounds_, false);
  }

  // Trigger saving the current session commands to file.
  void Save() {
    service()->Save();
  }

  // Synchronously reads the contents of the current session.
  std::vector<SessionWindow*>* ReadWindows() {
    // The session backend will post the callback as a task to whatever thread
    // called it. In our case, we don't want the main test thread to have tasks
    // posted to, so we perform the actual call to session service from the same
    // thread the work will be done on (backend_thread aka file thread). As a
    // result, it will directly call back, instead of posting a task, and we can
    // block on that callback.
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        NewRunnableMethod(this, &TestSessionService::DoReadWindows));

    // Wait for callback to happen.
    got_windows_.Wait();

    // By the time we reach here we've received the windows, so return them.
    return windows_;
  }

  // Makes the actual call to session service.
  // Lives on the file thread.
  void DoReadWindows() {
    if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
      LOG(ERROR) << "DoReadWindows called from wrong thread!";
      windows_ = NULL;
      got_windows_.Signal();
      return;
    }
    SessionService::SessionCallback* callback =
        NewCallback(this, &TestSessionService::OnGotSession);
    service()->GetCurrentSession(&consumer_, callback);
  }

  // Internal method used in the callback to obtain the current session.
  // Lives on and called from backend thread (file_thread).
  // We don't own windows so need to make a deep copy.
  void OnGotSession(int handle, std::vector<SessionWindow*>* windows) {
    // Hacky. We need to make a deep copy of the session windows. One way to do
    // this is to use the session model associators functionality to create
    // foreign sessions, which themselves wrap a SessionWindow vector. We just
    // need to make sure to destroy all the foreign sessions we created when
    // we're done. That's what the foreign_sessions_ ScopedVector is for.
    sync_pb::SessionSpecifics session;
    profile_->GetProfileSyncService()->
        GetSessionModelAssociator()->
        FillSpecificsFromSessions(windows, &session);

    std::vector<ForeignSession*> foreign_sessions;
    profile_->GetProfileSyncService()->
        GetSessionModelAssociator()->
        AppendForeignSessionFromSpecifics(&session, &foreign_sessions);
    ASSERT_EQ(foreign_sessions.size(), 1U);
    foreign_sessions_.push_back(foreign_sessions[0]);
    windows_ = &foreign_sessions[0]->windows;
    got_windows_.Signal();
  }

 private:
  ~TestSessionService() {
    ReleaseService();  // We don't own this, so don't destroy it.
  }

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
  explicit LiveSessionsSyncTest(TestType test_type)
      : LiveSyncTest(test_type),
        done_closing_(false, false) {}
  virtual ~LiveSessionsSyncTest() {}

  // Used to access the session service associated with a specific sync profile.
  SessionService* GetSessionService(int index) {
    return GetProfile(index)->GetSessionService();
  }

  // Used to access the session service test helper associated with a specific
  // sync profile.
  TestSessionService* GetHelper(int index) {
    return test_session_services_[index]->get();
  }

  // Used to access the browser associated with a specific sync profile.
  Browser* GetBrowser(int index) {
    return browsers_[index];
  }

  // Sets up the TestSessionService helper and the new browser windows.
  bool SetupClients() {
    if (!LiveSyncTest::SetupClients()) {
      return false;
    }

    // Go through and make the TestSessionServices and Browsers.
    for (int i = 0; i < num_clients(); ++i) {
      scoped_refptr<TestSessionService>* new_tester =
          new scoped_refptr<TestSessionService>;
      *new_tester = new TestSessionService(
          GetSessionService(i), GetProfile(i));
      test_session_services_.push_back(new_tester);
      GetHelper(i)->SetUp();

      browsers_.push_back(Browser::Create(GetProfile(i)));
    }

    return true;
  }

  // Open a single tab and return the TabContents. TabContents must be checked
  // to ensure the tab opened successsfully.
  TabContents* OpenTab(int index, GURL url) WARN_UNUSED_RESULT {
     TabContents* tab = GetBrowser(index)->
        AddSelectedTabWithURL(url, PageTransition::START_PAGE)->tab_contents();

     // Wait for the page to finish loading.
     ui_test_utils::WaitForNavigation(
         &GetBrowser(index)->GetSelectedTabContents()->controller());

     return tab;
  }

  // Creates and verifies the creation of a new window with one tab displaying
  // the specified GURL.
  // Returns: the SessionWindow associated with the new window.
  std::vector<SessionWindow*>* InitializeNewWindowWithTab(int index, GURL url)
      WARN_UNUSED_RESULT {
    if (!OpenTab(index, url)) {
      return NULL;
    }
    GetHelper(index)->Save();
    std::vector<SessionWindow*>* windows = GetHelper(index)->ReadWindows();
    if (windows->size() != 1) {
      LOG(ERROR) << "InitializeNewWindowWithTab called with open windows!";
      return NULL;
    }
    if (1U != (*windows)[0]->tabs.size())
      return NULL;
    SortSessionWindows(windows);
    return windows;
  }

  // Checks that window count and foreign session count are 0.
  bool CheckInitialState(int index) WARN_UNUSED_RESULT {
    if (0 != GetNumWindows(index))
      return false;
    if (0 != GetNumForeignSessions(index))
      return false;
    return true;
  }

  // Returns number of open windows for a profile.
  int GetNumWindows(int index) {
    // We don't own windows.
    std::vector<SessionWindow*>* windows = GetHelper(index)->ReadWindows();
    return windows->size();
  }

  // Returns number of foreign sessions for a profile.
  int GetNumForeignSessions(int index) {
    ScopedVector<ForeignSession> sessions;
    if (!GetProfile(index)->GetProfileSyncService()->
        GetSessionModelAssociator()->GetSessionData(&sessions.get()))
        return 0;
    return sessions.size();
  }

  // Fills the sessions vector with the model associator's foreign session data.
  // Caller owns sessions.
  bool GetSessionData(int index, std::vector<ForeignSession*>* sessions)
      WARN_UNUSED_RESULT {
    if (!GetProfile(index)->GetProfileSyncService()->
        GetSessionModelAssociator()->GetSessionData(sessions))
      return false;
    SortForeignSessions(sessions);
    return true;
  }

  // Compare session windows based on their first tab's url.
  // Returns true if the virtual url of the lhs is < the rhs.
  static bool CompareSessionWindows(SessionWindow* lhs, SessionWindow* rhs) {
    if (!lhs ||
        !rhs ||
        lhs->tabs.size() < 1 ||
        rhs->tabs.size() < 1 ||
        lhs->tabs[0]->navigations.size() < 1 ||
        rhs->tabs[0]->navigations.size() < 1) {
      // Catchall for uncomparable data.
      return false;
    }

    return lhs->tabs[0]->navigations[0].virtual_url() <
        rhs->tabs[0]->navigations[0].virtual_url();
  }

  // Sort session windows using our custom comparator (first tab url
  // comparison).
  void SortSessionWindows(std::vector<SessionWindow*>* windows) {
    std::sort(windows->begin(), windows->end(),
              LiveSessionsSyncTest::CompareSessionWindows);
  }

  // Compares a foreign session based on the first session window.
  // Returns true based on the comparison of the session windows.
  static bool CompareForeignSessions(ForeignSession* lhs, ForeignSession* rhs) {
    if (!lhs ||
        !rhs ||
        lhs->windows.size() < 1 ||
        rhs->windows.size() < 1) {
      // Catchall for uncomparable data.
      return false;
    }

    return CompareSessionWindows(lhs->windows[0], rhs->windows[0]);
  }

  // Sort a foreign session vector using our custom foreign session comparator.
  void SortForeignSessions(std::vector<ForeignSession*>* sessions) {
    std::sort(sessions->begin(), sessions->end(),
        LiveSessionsSyncTest::CompareForeignSessions);
  }

  // Verifies that two SessionWindows match.
  // Returns:
  //  - true if all the following match:
  //    1. number of SessionWindows per vector,
  //    2. number of tabs per SessionWindow,
  //    3. number of tab navigations per nab,
  //    4. actual tab navigations
  // - false otherwise.
  bool WindowsMatch(const std::vector<SessionWindow*> &win1,
      const std::vector<SessionWindow*> &win2) WARN_UNUSED_RESULT {
    SessionTab* client0_tab;
    SessionTab* client1_tab;
    if (win1.size() != win2.size())
      return false;
    for (size_t i = 0; i < win1.size(); ++i) {
      if (win1[i]->tabs.size() != win2[i]->tabs.size())
        return false;
      for (size_t j = 0; j < win1[i]->tabs.size(); ++j) {
        client0_tab = win1[i]->tabs[j];
        client1_tab = win2[i]->tabs[j];
        for (size_t k = 0; k < client0_tab->navigations.size(); ++k) {
          GetHelper(0)->AssertNavigationEquals(client0_tab->navigations[k],
                                               client1_tab->navigations[k]);
        }
      }
    }

    return true;
  }

  // Retrieves the foreign sessions for a particular profile and compares them
  // with a reference SessionWindow list.
  // Returns true if the session windows of the foreign session matches the
  // reference.
  bool CheckForeignSessionsAgainst(int index,
      const std::vector<std::vector<SessionWindow*>* >& windows)
      WARN_UNUSED_RESULT {
    ScopedVector<ForeignSession> sessions;
    if (!GetSessionData(index, &sessions.get()))
      return false;
    if ((size_t)(num_clients()-1) != sessions.size())
      return false;

    int window_index = 0;
    for (size_t j = 0; j < sessions->size(); ++j, ++window_index) {
      if (window_index == index)
        window_index++;  // Skip self.
      if (!WindowsMatch(sessions[j]->windows, *windows[window_index]))
        return false;
    }

    return true;
  }

 protected:
  // Clean up our mess.
  virtual void CleanUpOnMainThread() {
    // Close all browsers. We need to do this now, as opposed to letting the
    // test framework handle it, because we created our own browser for each
    // sync profile.
    BrowserList::CloseAllBrowsers();
    ui_test_utils::RunAllPendingInMessageLoop();

    // All browsers should be closed at this point, else when the framework
    // calls QuitBrowsers() we could see memory corruption.
    ASSERT_EQ(0U, BrowserList::size());

    LiveSyncTest::CleanUpOnMainThread();
  }

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

