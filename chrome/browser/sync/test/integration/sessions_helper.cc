// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sessions_helper.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sessions/notification_service_sessions_router.h"
#include "chrome/browser/sync/sessions/sessions_sync_manager.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "url/gurl.h"

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
  return ProfileSyncServiceFactory::GetInstance()->GetForProfile(
      test()->GetProfile(index))->GetOpenTabsUIDelegate()->
          GetLocalSession(session);
}

bool ModelAssociatorHasTabWithUrl(int index, const GURL& url) {
  content::RunAllPendingInMessageLoop();
  const browser_sync::SyncedSession* local_session;
  if (!GetLocalSession(index, &local_session)) {
    return false;
  }

  if (local_session->windows.size() == 0) {
    DVLOG(1) << "Empty windows vector";
    return false;
  }

  int nav_index;
  sessions::SerializedNavigationEntry nav;
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
        DVLOG(1) << "Timestamp is " << nav.timestamp().ToInternalValue();
        if (nav.title().empty()) {
          DVLOG(1) << "Title empty -- tab hasn't finished loading yet";
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
  chrome::ShowSingletonTab(test()->GetBrowser(index), url);
  return WaitForTabsToLoad(index, std::vector<GURL>(1, url));
}

bool OpenMultipleTabs(int index, const std::vector<GURL>& urls) {
  Browser* browser = test()->GetBrowser(index);
  for (std::vector<GURL>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    DVLOG(1) << "Opening tab: " << it->spec() << " using profile " << index
             << ".";
    chrome::ShowSingletonTab(browser, *it);
  }
  return WaitForTabsToLoad(index, urls);
}

namespace {

class TabEventHandler : public browser_sync::LocalSessionEventHandler {
 public:
  TabEventHandler() : weak_factory_(this) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TabEventHandler::QuitLoop, weak_factory_.GetWeakPtr()),
        TestTimeouts::action_max_timeout());
  }

  virtual void OnLocalTabModified(
      browser_sync::SyncedTabDelegate* modified_tab) OVERRIDE {
    // Unwind to ensure SessionsSyncManager has processed the event.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TabEventHandler::QuitLoop, weak_factory_.GetWeakPtr()));
  }

  virtual void OnFaviconPageUrlsUpdated(
      const std::set<GURL>& updated_page_urls) OVERRIDE {
    // Unwind to ensure SessionsSyncManager has processed the event.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TabEventHandler::QuitLoop, weak_factory_.GetWeakPtr()));
  }

 private:
  void QuitLoop() {
    base::MessageLoop::current()->Quit();
  }

  base::WeakPtrFactory<TabEventHandler> weak_factory_;
};

}  // namespace

bool WaitForTabsToLoad(int index, const std::vector<GURL>& urls) {
  DVLOG(1) << "Waiting for session to propagate to associator.";
  base::TimeTicks start_time = base::TimeTicks::Now();
  base::TimeTicks end_time = start_time + TestTimeouts::action_max_timeout();
  bool found;
  for (std::vector<GURL>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    found = false;
    while (!found) {
      found = ModelAssociatorHasTabWithUrl(index, *it);
      if (base::TimeTicks::Now() >= end_time) {
        LOG(ERROR) << "Failed to find all tabs after "
                   << TestTimeouts::action_max_timeout().InSecondsF()
                   << " seconds.";
        return false;
      }
      if (!found) {
        TabEventHandler handler;
        browser_sync::NotificationServiceSessionsRouter router(
            test()->GetProfile(index),
            syncer::SyncableService::StartSyncFlare());
        router.StartRoutingTo(&handler);
        content::RunMessageLoop();
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
  if (!ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          test()->GetProfile(index))->
          GetOpenTabsUIDelegate()->GetAllForeignSessions(
              &sessions)) {
    return 0;
  }
  return sessions.size();
}

bool GetSessionData(int index, SyncedSessionVector* sessions) {
  if (!ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          test()->GetProfile(index))->
          GetOpenTabsUIDelegate()->GetAllForeignSessions(
              sessions)) {
    return false;
  }
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

bool NavigationEquals(const sessions::SerializedNavigationEntry& expected,
                      const sessions::SerializedNavigationEntry& actual) {
  if (expected.virtual_url() != actual.virtual_url()) {
    LOG(ERROR) << "Expected url " << expected.virtual_url()
               << ", actual " << actual.virtual_url();
    return false;
  }
  if (expected.referrer().url != actual.referrer().url) {
    LOG(ERROR) << "Expected referrer "
               << expected.referrer().url
               << ", actual "
               << actual.referrer().url;
    return false;
  }
  if (expected.title() != actual.title()) {
    LOG(ERROR) << "Expected title " << expected.title()
               << ", actual " << actual.title();
    return false;
  }
  if (expected.transition_type() != actual.transition_type()) {
    LOG(ERROR) << "Expected transition "
               << expected.transition_type()
               << ", actual "
               << actual.transition_type();
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

namespace {

// Helper class used in the implementation of AwaitCheckForeignSessionsAgainst.
class CheckForeignSessionsChecker : public MultiClientStatusChangeChecker {
 public:
  CheckForeignSessionsChecker(int index,
                              const std::vector<ScopedWindowMap>& windows);
  virtual ~CheckForeignSessionsChecker();

  virtual bool IsExitConditionSatisfied() OVERRIDE;
  virtual std::string GetDebugMessage() const OVERRIDE;
 private:
  int index_;
  const std::vector<ScopedWindowMap>& windows_;
};

CheckForeignSessionsChecker::CheckForeignSessionsChecker(
    int index, const std::vector<ScopedWindowMap>& windows)
    : MultiClientStatusChangeChecker(
        sync_datatype_helper::test()->GetSyncServices()),
      index_(index),
      windows_(windows) {}

CheckForeignSessionsChecker::~CheckForeignSessionsChecker() {}

bool CheckForeignSessionsChecker::IsExitConditionSatisfied() {
  return CheckForeignSessionsAgainst(index_, windows_);
}

std::string CheckForeignSessionsChecker::GetDebugMessage() const {
  return "Waiting for matching foreign sessions";
}

}  //  namespace

bool AwaitCheckForeignSessionsAgainst(
    int index, const std::vector<ScopedWindowMap>& windows) {
  CheckForeignSessionsChecker checker(index, windows);
  checker.Wait();
  return !checker.TimedOut();
}

void DeleteForeignSession(int index, std::string session_tag) {
  ProfileSyncServiceFactory::GetInstance()->GetForProfile(
      test()->GetProfile(index))->
          GetOpenTabsUIDelegate()->DeleteForeignSession(session_tag);
}

}  // namespace sessions_helper
