// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_sessions_sync_test.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/test/ui_test_utils.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

TestSessionService::TestSessionService()
    : SessionServiceTestHelper(),
      done_saving_(false, false),
      got_windows_(false, false),
      profile_(NULL),
      window_bounds_(0, 1, 2, 3) {}

TestSessionService::TestSessionService(SessionService* service,
                                       Profile* profile)
    : SessionServiceTestHelper(service),
      done_saving_(false, false),
      got_windows_(false, false),
      profile_(profile),
      window_bounds_(0, 1, 2, 3) {}

void TestSessionService::SetUp() {
  ASSERT_TRUE(service()) << "SetUp() called without setting SessionService";
  ASSERT_TRUE(profile_);
  service()->SetWindowType(window_id_, Browser::TYPE_NORMAL);
  service()->SetWindowBounds(window_id_, window_bounds_, false);
}

void TestSessionService::Save() {
  service()->Save();
}

std::vector<SessionWindow*>* TestSessionService::ReadWindows() {
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


void TestSessionService::DoReadWindows() {
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

void TestSessionService::OnGotSession(int handle,
                                      std::vector<SessionWindow*>* windows) {
  scoped_ptr<ForeignSession> foreign_session(new ForeignSession());
  for (size_t w = 0; w < windows->size(); ++w) {
    const SessionWindow& window = *windows->at(w);
    scoped_ptr<SessionWindow> new_window(new SessionWindow());
    for (size_t t = 0; t < window.tabs.size(); ++t) {
      const SessionTab& tab = *window.tabs.at(t);
      scoped_ptr<SessionTab> new_tab(new SessionTab());
      new_tab->navigations.resize(tab.navigations.size());
      std::copy(tab.navigations.begin(), tab.navigations.end(),
                new_tab->navigations.begin());
      new_window->tabs.push_back(new_tab.release());
    }
    foreign_session->windows.push_back(new_window.release());
  }
  windows_ = &(foreign_session->windows);
  foreign_sessions_.push_back(foreign_session.release());
  got_windows_.Signal();
}

TestSessionService::~TestSessionService() {
  ReleaseService();  // We don't own this, so don't destroy it.
}

LiveSessionsSyncTest::LiveSessionsSyncTest(TestType test_type)
    : LiveSyncTest(test_type),
      done_closing_(false, false) {}

LiveSessionsSyncTest::~LiveSessionsSyncTest() {}

SessionService* LiveSessionsSyncTest::GetSessionService(int index) {
  return GetProfile(index)->GetSessionService();
}

TestSessionService* LiveSessionsSyncTest::GetHelper(int index) {
  return test_session_services_[index]->get();
}

Browser* LiveSessionsSyncTest::GetBrowser(int index) {
  return browsers_[index];
}

bool LiveSessionsSyncTest::SetupClients() {
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

TabContents* LiveSessionsSyncTest::OpenTab(int index, GURL url) {
  TabContents* tab =
      GetBrowser(index)->
      AddSelectedTabWithURL(url, PageTransition::START_PAGE)->tab_contents();

  // Wait for the page to finish loading.
  ui_test_utils::WaitForNavigation(
      &GetBrowser(index)->GetSelectedTabContents()->controller());

  return tab;
}

std::vector<SessionWindow*>* LiveSessionsSyncTest::InitializeNewWindowWithTab(
    int index, GURL url) {
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

bool LiveSessionsSyncTest::CheckInitialState(int index) {
  if (0 != GetNumWindows(index))
    return false;
  if (0 != GetNumForeignSessions(index))
    return false;
  return true;
}

int LiveSessionsSyncTest::GetNumWindows(int index) {
  // We don't own windows.
  std::vector<SessionWindow*>* windows = GetHelper(index)->ReadWindows();
  return windows->size();
}

int LiveSessionsSyncTest::GetNumForeignSessions(int index) {
  std::vector<const ForeignSession*> sessions;
  if (!GetProfile(index)->GetProfileSyncService()->
      GetSessionModelAssociator()->GetAllForeignSessions(&sessions))
    return 0;
  return sessions.size();
}

bool LiveSessionsSyncTest::GetSessionData(
    int index,
    std::vector<const ForeignSession*>* sessions) {
  if (!GetProfile(index)->GetProfileSyncService()->
      GetSessionModelAssociator()->GetAllForeignSessions(sessions))
    return false;
  SortForeignSessions(sessions);
  return true;
}

// static
bool LiveSessionsSyncTest::CompareSessionWindows(SessionWindow* lhs,
                                                 SessionWindow* rhs) {
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

void LiveSessionsSyncTest::SortSessionWindows(
    std::vector<SessionWindow*>* windows) {
  std::sort(windows->begin(), windows->end(),
            LiveSessionsSyncTest::CompareSessionWindows);
}

//static
bool LiveSessionsSyncTest::CompareForeignSessions(
    const ForeignSession* lhs,
    const ForeignSession* rhs) {
  if (!lhs ||
      !rhs ||
      lhs->windows.size() < 1 ||
      rhs->windows.size() < 1) {
    // Catchall for uncomparable data.
    return false;
  }

  return CompareSessionWindows(lhs->windows[0], rhs->windows[0]);
}

void LiveSessionsSyncTest::SortForeignSessions(
    std::vector<const ForeignSession*>* sessions) {
  std::sort(sessions->begin(), sessions->end(),
            LiveSessionsSyncTest::CompareForeignSessions);
}

bool LiveSessionsSyncTest::NavigationEquals(const TabNavigation& expected,
                                            const TabNavigation& actual) {
  if (expected.virtual_url() != actual.virtual_url())
    return false;
  if (expected.referrer() != actual.referrer())
    return false;
  if (expected.title() != actual.title())
    return false;
  if (expected.transition() != actual.transition())
    return false;
  return true;
}

bool LiveSessionsSyncTest::WindowsMatch(
    const std::vector<SessionWindow*> &win1,
    const std::vector<SessionWindow*> &win2) {
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
        if (!NavigationEquals(client0_tab->navigations[k],
                              client1_tab->navigations[k])) {
          return false;
        }
      }
    }
  }

  return true;
}

bool LiveSessionsSyncTest::CheckForeignSessionsAgainst(
    int index,
    const std::vector<std::vector<SessionWindow*>* >& windows) {
  std::vector<const ForeignSession*> sessions;
  if (!GetSessionData(index, &sessions))
    return false;
  if ((size_t)(num_clients()-1) != sessions.size())
    return false;

  int window_index = 0;
  for (size_t j = 0; j < sessions.size(); ++j, ++window_index) {
    if (window_index == index)
      window_index++;  // Skip self.
    if (!WindowsMatch(sessions[j]->windows, *windows[window_index]))
      return false;
  }

  return true;
}

void LiveSessionsSyncTest::CleanUpOnMainThread() {
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
