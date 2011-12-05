// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sessions_helper.h"

#include <algorithm>

#include "base/stl_util.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

using sync_datatype_helper::test;

namespace sessions_helper {

ScopedWindowMap::ScopedWindowMap() {
}

ScopedWindowMap::ScopedWindowMap(SessionWindowMap* windows) {
  Reset(windows);
}

ScopedWindowMap::~ScopedWindowMap() {
  STLDeleteContainerPairSecondPointers(windows_.begin(), windows_.end());
}

SessionWindowMap* ScopedWindowMap::GetMutable() {
  return &windows_;
}

const SessionWindowMap* ScopedWindowMap::Get() const {
  return &windows_;
}

void ScopedWindowMap::Reset(SessionWindowMap* windows) {
  STLDeleteContainerPairSecondPointers(windows_.begin(), windows_.end());
  windows_.clear();
  std::swap(*windows, windows_);
}

bool GetLocalSession(int index, const browser_sync::SyncedSession** session) {
  return test()->GetProfile(index)->GetProfileSyncService()->
      GetSessionModelAssociator()->GetLocalSession(session);
}

bool ModelAssociatorHasTabWithUrl(int index, const GURL& url) {
  ui_test_utils::RunAllPendingInMessageLoop();
  const browser_sync::SyncedSession* local_session;
  if (!GetLocalSession(index, &local_session)) {
    return false;
  }

  if (local_session->windows.size() == 0) {
    DVLOG(1) << "Empty windows vector";
    return false;
  }

  int nav_index;
  TabNavigation nav;
  for (SessionWindowMap::const_iterator it =
           local_session->windows.begin();
       it != local_session->windows.end(); ++it) {
    if (it->second->tabs.size() == 0) {
      DVLOG(1) << "Empty tabs vector";
      continue;
    }
    for (std::vector<SessionTab*>::const_iterator tab_it =
             it->second->tabs.begin();
         tab_it != it->second->tabs.end(); ++tab_it) {
      if ((*tab_it)->navigations.size() == 0) {
        DVLOG(1) << "Empty navigations vector";
        continue;
      }
      nav_index = (*tab_it)->current_navigation_index;
      nav = (*tab_it)->navigations[nav_index];
      if (nav.virtual_url() == url) {
        DVLOG(1) << "Found tab with url " << url.spec();
        if (nav.title().empty()) {
          DVLOG(1) << "No title!";
          continue;
        }
        return true;
      }
    }
  }
  DVLOG(1) << "Could not find tab with url " << url.spec();
  return false;
}

bool OpenTab(int index, const GURL& url) {
  DVLOG(1) << "Opening tab: " << url.spec() << " using profile "
           << index << ".";
  test()->GetBrowser(index)->ShowSingletonTab(url);
  return WaitForTabsToLoad(index, std::vector<GURL>(1, url));
}

bool OpenMultipleTabs(int index, const std::vector<GURL>& urls) {
  Browser* browser = test()->GetBrowser(index);
  for (std::vector<GURL>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    DVLOG(1) << "Opening tab: " << it->spec() << " using profile " << index
             << ".";
    browser->ShowSingletonTab(*it);
  }
  return WaitForTabsToLoad(index, urls);
}

bool WaitForTabsToLoad(int index, const std::vector<GURL>& urls) {
  DVLOG(1) << "Waiting for session to propagate to associator.";
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
        test()->GetProfile(index)->GetProfileSyncService()->
            GetSessionModelAssociator()->
            BlockUntilLocalChangeForTest(timeout_milli);
        ui_test_utils::RunMessageLoop();
      }
    }
  }
  return true;
}

bool GetLocalWindows(int index, SessionWindowMap* local_windows) {
  // The local session provided by GetLocalSession is owned, and has lifetime
  // controlled, by the model associator, so we must make our own copy.
  const browser_sync::SyncedSession* local_session;
  if (!GetLocalSession(index, &local_session)) {
    return false;
  }
  for (SessionWindowMap::const_iterator w = local_session->windows.begin();
       w != local_session->windows.end(); ++w) {
    const SessionWindow& window = *(w->second);
    SessionWindow* new_window = new SessionWindow();
    new_window->window_id.set_id(window.window_id.id());
    for (size_t t = 0; t < window.tabs.size(); ++t) {
      const SessionTab& tab = *window.tabs.at(t);
      SessionTab* new_tab = new SessionTab();
      new_tab->navigations.resize(tab.navigations.size());
      std::copy(tab.navigations.begin(), tab.navigations.end(),
                new_tab->navigations.begin());
      new_window->tabs.push_back(new_tab);
    }
    (*local_windows)[new_window->window_id.id()] = new_window;
  }

  return true;
}

bool OpenTabAndGetLocalWindows(int index,
                               const GURL& url,
                               SessionWindowMap* local_windows) {
  if (!OpenTab(index, url)) {
    return false;
  }
  return GetLocalWindows(index, local_windows);
}

bool CheckInitialState(int index) {
  if (0 != GetNumWindows(index))
    return false;
  if (0 != GetNumForeignSessions(index))
    return false;
  return true;
}

int GetNumWindows(int index) {
  const browser_sync::SyncedSession* local_session;
  if (!GetLocalSession(index, &local_session)) {
    return 0;
  }
  return local_session->windows.size();
}

int GetNumForeignSessions(int index) {
  SyncedSessionVector sessions;
  if (!test()->GetProfile(index)->GetProfileSyncService()->
          GetSessionModelAssociator()->GetAllForeignSessions(&sessions))
    return 0;
  return sessions.size();
}

bool GetSessionData(int index, SyncedSessionVector* sessions) {
  if (!test()->GetProfile(index)->GetProfileSyncService()->
          GetSessionModelAssociator()->GetAllForeignSessions(sessions))
    return false;
  SortSyncedSessions(sessions);
  return true;
}

bool CompareSyncedSessions(const browser_sync::SyncedSession* lhs,
                           const browser_sync::SyncedSession* rhs) {
  if (!lhs ||
      !rhs ||
      lhs->windows.size() < 1 ||
      rhs->windows.size() < 1) {
    // Catchall for uncomparable data.
    return false;
  }

  return lhs->windows < rhs->windows;
}

void SortSyncedSessions(SyncedSessionVector* sessions) {
  std::sort(sessions->begin(), sessions->end(),
            CompareSyncedSessions);
}

bool NavigationEquals(const TabNavigation& expected,
                      const TabNavigation& actual) {
  if (expected.virtual_url() != actual.virtual_url()) {
    LOG(ERROR) << "Expected url " << expected.virtual_url()
               << ", actual " << actual.virtual_url();
    return false;
  }
  if (expected.referrer().url != actual.referrer().url) {
    LOG(ERROR) << "Expected referrer " << expected.referrer().url
               << ", actual " << actual.referrer().url;
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

bool WindowsMatch(const SessionWindowMap& win1,
                  const SessionWindowMap& win2) {
  SessionTab* client0_tab;
  SessionTab* client1_tab;
  if (win1.size() != win2.size())
    return false;
  for (SessionWindowMap::const_iterator i = win1.begin();
       i != win1.end(); ++i) {
    SessionWindowMap::const_iterator j = win2.find(i->first);
    if (j == win2.end())
      return false;
    if (i->second->tabs.size() != j->second->tabs.size())
      return false;
    for (size_t t = 0; t < i->second->tabs.size(); ++t) {
      client0_tab = i->second->tabs[t];
      client1_tab = j->second->tabs[t];
      for (size_t n = 0; n < client0_tab->navigations.size(); ++n) {
        if (!NavigationEquals(client0_tab->navigations[n],
                              client1_tab->navigations[n])) {
          return false;
        }
      }
    }
  }

  return true;
}

bool CheckForeignSessionsAgainst(
    int index,
    const std::vector<ScopedWindowMap>& windows) {
  SyncedSessionVector sessions;
  if (!GetSessionData(index, &sessions))
    return false;
  if ((size_t)(test()->num_clients()-1) != sessions.size())
    return false;

  int window_index = 0;
  for (size_t j = 0; j < sessions.size(); ++j, ++window_index) {
    if (window_index == index)
      window_index++;  // Skip self.
    if (!WindowsMatch(sessions[j]->windows,
                      *(windows[window_index].Get())))
      return false;
  }

  return true;
}

void DeleteForeignSession(int index, std::string session_tag) {
  test()->GetProfile(index)->GetProfileSyncService()->
      GetSessionModelAssociator()->DeleteForeignSession(session_tag);
}

}  // namespace sessions_helper
