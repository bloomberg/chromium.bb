// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_sessions_sync_test.h"

#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

LiveSessionsSyncTest::LiveSessionsSyncTest(TestType test_type)
    : LiveSyncTest(test_type),
      done_closing_(false, false) {}

LiveSessionsSyncTest::~LiveSessionsSyncTest() {}

Browser* LiveSessionsSyncTest::GetBrowser(int index) {
  return browsers_[index];
}

bool LiveSessionsSyncTest::SetupClients() {
  if (!LiveSyncTest::SetupClients()) {
    return false;
  }

  // Go through and make the Browsers.
  for (int i = 0; i < num_clients(); ++i) {
    browsers_.push_back(Browser::Create(GetProfile(i)));
  }

  return true;
}

bool LiveSessionsSyncTest::GetLocalSession(int index,
                                           const SyncedSession** session) {
  return GetProfile(index)->GetProfileSyncService()->
      GetSessionModelAssociator()->GetLocalSession(session);
}

bool LiveSessionsSyncTest::ModelAssociatorHasTabWithUrl(int index,
                                                        const GURL& url) {
  ui_test_utils::RunAllPendingInMessageLoop();
  const SyncedSession* local_session;
  if (!GetLocalSession(index, &local_session)) {
    return false;
  }

  if (local_session->windows.size() == 0) {
    VLOG(1) << "Empty windows vector";
    return false;
  }

  int nav_index;
  TabNavigation nav;
  for (std::vector<SessionWindow*>::const_iterator it =
           local_session->windows.begin(); it != local_session->windows.end();
           ++it) {
    if ((*it)->tabs.size() == 0) {
      VLOG(1) << "Empty tabs vector";
      continue;
    }
    for (std::vector<SessionTab*>::const_iterator tab_it =
             (*it)->tabs.begin(); tab_it != (*it)->tabs.end(); ++tab_it) {
      if ((*tab_it)->navigations.size() == 0) {
        VLOG(1) << "Empty navigations vector";
        continue;
      }
      nav_index = (*tab_it)->current_navigation_index;
      nav = (*tab_it)->navigations[nav_index];
      if (nav.virtual_url() == url) {
        VLOG(1) << "Found tab with url " << url.spec();
        if (nav.title().empty()) {
          VLOG(1) << "No title!";
          continue;
        }
        return true;
      }
    }
  }
  VLOG(1) << "Could not find tab with url " << url.spec();
  return false;
}

bool LiveSessionsSyncTest::OpenTab(int index, const GURL& url) {
  VLOG(1) << "Opening tab: " << url.spec() << " using profile " << index << ".";
  GetBrowser(index)->ShowSingletonTab(url);
  return WaitForTabsToLoad(index, std::vector<GURL>(1, url));
}

bool LiveSessionsSyncTest::OpenMultipleTabs(int index,
                                            const std::vector<GURL>& urls) {
  Browser* browser = GetBrowser(index);
  for (std::vector<GURL>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    VLOG(1) << "Opening tab: " << it->spec() << " using profile " << index
            << ".";
    browser->ShowSingletonTab(*it);
  }
  return WaitForTabsToLoad(index, urls);
}

bool LiveSessionsSyncTest::WaitForTabsToLoad(
    int index, const std::vector<GURL>& urls) {
  VLOG(1) << "Waiting for session to propagate to associator.";
  static const int timeout_milli = TestTimeouts::action_max_timeout_ms();
  base::TimeTicks start_time = base::TimeTicks::Now();
  base::TimeTicks end_time = start_time +
                             base::TimeDelta::FromMilliseconds(timeout_milli);
  bool found;
  for (std::vector<GURL>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    found = false;
    while (!found) {
      found = ModelAssociatorHasTabWithUrl(index, *it);
      if (base::TimeTicks::Now() >= end_time) {
        LOG(ERROR) << "Failed to find all tabs after " << timeout_milli/1000.0
                   << " seconds.";
        return false;
      }
      if (!found) {
        GetProfile(index)->GetProfileSyncService()->
            GetSessionModelAssociator()->
            BlockUntilLocalChangeForTest(timeout_milli);
        ui_test_utils::RunMessageLoop();
      }
    }
  }
  return true;
}

std::vector<SessionWindow*>* LiveSessionsSyncTest::GetLocalWindows(int index) {
  // The local session provided by GetLocalSession is owned, and has lifetime
  // controlled, by the model associator, so we must make our own copy.
  const SyncedSession* local_session;
  if (!GetLocalSession(index, &local_session)) {
    return NULL;
  }
  scoped_ptr<SyncedSession> session_copy(new SyncedSession());
  for (size_t w = 0; w < local_session->windows.size(); ++w) {
    const SessionWindow& window = *local_session->windows.at(w);
    scoped_ptr<SessionWindow> new_window(new SessionWindow());
    for (size_t t = 0; t < window.tabs.size(); ++t) {
      const SessionTab& tab = *window.tabs.at(t);
      scoped_ptr<SessionTab> new_tab(new SessionTab());
      new_tab->navigations.resize(tab.navigations.size());
      std::copy(tab.navigations.begin(), tab.navigations.end(),
                new_tab->navigations.begin());
      new_window->tabs.push_back(new_tab.release());
    }
    session_copy->windows.push_back(new_window.release());
  }
  std::vector<SessionWindow*>* windows = &session_copy->windows;
  session_copies.push_back(session_copy.release());

  // Sort the windows so their order is deterministic.
  SortSessionWindows(windows);

  return windows;
}

std::vector<SessionWindow*>* LiveSessionsSyncTest::InitializeNewWindowWithTab(
    int index, const GURL& url) {
  if (!OpenTab(index, url)) {
    return NULL;
  }
  return GetLocalWindows(index);
}

bool LiveSessionsSyncTest::CheckInitialState(int index) {
  if (0 != GetNumWindows(index))
    return false;
  if (0 != GetNumForeignSessions(index))
    return false;
  return true;
}

int LiveSessionsSyncTest::GetNumWindows(int index) {
  const SyncedSession* local_session;
  if (!GetLocalSession(index, &local_session)) {
    return 0;
  }
  return local_session->windows.size();
}

int LiveSessionsSyncTest::GetNumForeignSessions(int index) {
  std::vector<const SyncedSession*> sessions;
  if (!GetProfile(index)->GetProfileSyncService()->
          GetSessionModelAssociator()->GetAllForeignSessions(&sessions))
    return 0;
  return sessions.size();
}

bool LiveSessionsSyncTest::GetSessionData(
    int index,
    std::vector<const SyncedSession*>* sessions) {
  if (!GetProfile(index)->GetProfileSyncService()->
          GetSessionModelAssociator()->GetAllForeignSessions(sessions))
    return false;
  SortSyncedSessions(sessions);
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

// static
bool LiveSessionsSyncTest::CompareSyncedSessions(
    const SyncedSession* lhs,
    const SyncedSession* rhs) {
  if (!lhs ||
      !rhs ||
      lhs->windows.size() < 1 ||
      rhs->windows.size() < 1) {
    // Catchall for uncomparable data.
    return false;
  }

  return CompareSessionWindows(lhs->windows[0], rhs->windows[0]);
}

void LiveSessionsSyncTest::SortSyncedSessions(
    std::vector<const SyncedSession*>* sessions) {
  std::sort(sessions->begin(), sessions->end(),
            LiveSessionsSyncTest::CompareSyncedSessions);
}

bool LiveSessionsSyncTest::NavigationEquals(const TabNavigation& expected,
                                            const TabNavigation& actual) {
  if (expected.virtual_url() != actual.virtual_url()) {
    LOG(ERROR) << "Expected url " << expected.virtual_url()
               << ", actual " << actual.virtual_url();
    return false;
  }
  if (expected.referrer() != actual.referrer()) {
    LOG(ERROR) << "Expected referrer " << expected.referrer()
               << ", actual " << actual.referrer();
    return false;
  }
  if (expected.title() != actual.title()) {
    LOG(ERROR) << "Expected title " << expected.title()
               << ", actual " << actual.title();
    return false;
  }
  if (expected.transition() != actual.transition()) {
    LOG(ERROR) << "Expected transition " << expected.transition()
               << ", actual " << actual.transition();
    return false;
  }
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
  std::vector<const SyncedSession*> sessions;
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
