// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sessions_helper.h"

#include <stddef.h>

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sessions/notification_service_sessions_router.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/chrome_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

using sync_datatype_helper::test;

namespace sessions_helper {

bool GetLocalSession(int index, const sync_sessions::SyncedSession** session) {
  return ProfileSyncServiceFactory::GetInstance()->GetForProfile(
      test()->GetProfile(index))->GetOpenTabsUIDelegate()->
          GetLocalSession(session);
}

bool ModelAssociatorHasTabWithUrl(int index, const GURL& url) {
  content::RunAllPendingInMessageLoop();
  const sync_sessions::SyncedSession* local_session;
  if (!GetLocalSession(index, &local_session)) {
    return false;
  }

  if (local_session->windows.size() == 0) {
    DVLOG(1) << "Empty windows vector";
    return false;
  }

  int nav_index;
  sessions::SerializedNavigationEntry nav;
  for (auto it = local_session->windows.begin();
       it != local_session->windows.end(); ++it) {
    if (it->second->tabs.size() == 0) {
      DVLOG(1) << "Empty tabs vector";
      continue;
    }
    for (auto tab_it = it->second->tabs.begin();
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

class TabEventHandler : public sync_sessions::LocalSessionEventHandler {
 public:
  TabEventHandler() : weak_factory_(this) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TabEventHandler::QuitLoop, weak_factory_.GetWeakPtr()),
        TestTimeouts::action_max_timeout());
  }

  void OnLocalTabModified(
      sync_sessions::SyncedTabDelegate* modified_tab) override {
    // Unwind to ensure SessionsSyncManager has processed the event.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&TabEventHandler::QuitLoop, weak_factory_.GetWeakPtr()));
  }

  void OnFaviconsChanged(const std::set<GURL>& /* page_urls */,
                         const GURL& /* icon_url */) override {
    // Unwind to ensure SessionsSyncManager has processed the event.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&TabEventHandler::QuitLoop, weak_factory_.GetWeakPtr()));
  }

 private:
  void QuitLoop() { base::MessageLoop::current()->QuitWhenIdle(); }

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
        sync_sessions::NotificationServiceSessionsRouter router(
            test()->GetProfile(index),
            ProfileSyncServiceFactory::GetInstance()
                ->GetForProfile(test()->GetProfile(index))
                ->GetSyncClient()
                ->GetSyncSessionsClient(),
            syncer::SyncableService::StartSyncFlare());
        router.StartRoutingTo(&handler);
        content::RunMessageLoop();
      }
    }
  }
  return true;
}

bool GetLocalWindows(int index, ScopedWindowMap* local_windows) {
  // The local session provided by GetLocalSession is owned, and has lifetime
  // controlled, by the model associator, so we must make our own copy.
  const sync_sessions::SyncedSession* local_session;
  if (!GetLocalSession(index, &local_session)) {
    return false;
  }
  for (auto w = local_session->windows.begin();
       w != local_session->windows.end(); ++w) {
    const sessions::SessionWindow& window = *(w->second);
    std::unique_ptr<sessions::SessionWindow> new_window =
        base::MakeUnique<sessions::SessionWindow>();
    new_window->window_id.set_id(window.window_id.id());
    for (size_t t = 0; t < window.tabs.size(); ++t) {
      const sessions::SessionTab& tab = *window.tabs.at(t);
      std::unique_ptr<sessions::SessionTab> new_tab =
          base::MakeUnique<sessions::SessionTab>();
      new_tab->navigations.resize(tab.navigations.size());
      std::copy(tab.navigations.begin(), tab.navigations.end(),
                new_tab->navigations.begin());
      new_window->tabs.push_back(std::move(new_tab));
    }
    auto id = new_window->window_id.id();
    (*local_windows)[id] = std::move(new_window);
  }

  return true;
}

bool OpenTabAndGetLocalWindows(int index,
                               const GURL& url,
                               ScopedWindowMap* local_windows) {
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
  const sync_sessions::SyncedSession* local_session;
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

bool CompareSyncedSessions(const sync_sessions::SyncedSession* lhs,
                           const sync_sessions::SyncedSession* rhs) {
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
  if (expected.referrer_url() != actual.referrer_url()) {
    LOG(ERROR) << "Expected referrer "
               << expected.referrer_url()
               << ", actual "
               << actual.referrer_url();
    return false;
  }
  if (expected.title() != actual.title()) {
    LOG(ERROR) << "Expected title " << expected.title()
               << ", actual " << actual.title();
    return false;
  }
  if (!ui::PageTransitionTypeIncludingQualifiersIs(expected.transition_type(),
                                                   actual.transition_type())) {
    LOG(ERROR) << "Expected transition "
               << expected.transition_type()
               << ", actual "
               << actual.transition_type();
    return false;
  }
  return true;
}

namespace {

template <typename T1, typename T2>
bool WindowsMatchImpl(const T1& win1, const T2& win2) {
  sessions::SessionTab* client0_tab;
  sessions::SessionTab* client1_tab;
  if (win1.size() != win2.size()) {
    LOG(ERROR) << "Win size doesn't match, win1 size: "
        << win1.size()
        << ", win2 size: "
        << win2.size();
    return false;
  }
  for (auto i = win1.begin(); i != win1.end(); ++i) {
    auto j = win2.find(i->first);
    if (j == win2.end()) {
      LOG(ERROR) << "Session doesn't match";
      return false;
    }
    if (i->second->tabs.size() != j->second->tabs.size()) {
      LOG(ERROR) << "Tab size doesn't match, tab1 size: "
          << i->second->tabs.size()
          << ", tab2 size: "
          << j->second->tabs.size();
      return false;
    }
    for (size_t t = 0; t < i->second->tabs.size(); ++t) {
      client0_tab = i->second->tabs[t].get();
      client1_tab = j->second->tabs[t].get();
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

}  // namespace

bool WindowsMatch(const ScopedWindowMap& win1, const ScopedWindowMap& win2) {
  return WindowsMatchImpl(win1, win2);
}

bool WindowsMatch(const SessionWindowMap& win1, const ScopedWindowMap& win2) {
  return WindowsMatchImpl(win1, win2);
}

bool CheckForeignSessionsAgainst(
    int index,
    const std::vector<ScopedWindowMap>& windows) {
  SyncedSessionVector sessions;

  if (!GetSessionData(index, &sessions)) {
    LOG(ERROR) << "Cannot get session data";
    return false;
  }

  for (size_t w_index = 0; w_index < windows.size(); ++w_index) {
    // Skip the client's local window
    if (static_cast<int>(w_index) == index)
      continue;

    size_t s_index = 0;

    for (; s_index < sessions.size(); ++s_index) {
      if (WindowsMatch(sessions[s_index]->windows, windows[w_index]))
        break;
    }

    if (s_index == sessions.size()) {
      LOG(ERROR) << "Cannot find window #" << w_index;
      return false;
    }
  }

  return true;
}

void DeleteForeignSession(int index, std::string session_tag) {
  ProfileSyncServiceFactory::GetInstance()
      ->GetForProfile(test()->GetProfile(index))
      ->GetOpenTabsUIDelegate()
      ->DeleteForeignSession(session_tag);
}

}  // namespace sessions_helper

ForeignSessionsMatchChecker::ForeignSessionsMatchChecker(
    int index,
    const std::vector<sessions_helper::ScopedWindowMap>& windows)
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()),
      index_(index),
      windows_(windows) {}

bool ForeignSessionsMatchChecker::IsExitConditionSatisfied() {
  return sessions_helper::CheckForeignSessionsAgainst(index_, windows_);
}

std::string ForeignSessionsMatchChecker::GetDebugMessage() const {
  return "Waiting for matching foreign sessions";
}
