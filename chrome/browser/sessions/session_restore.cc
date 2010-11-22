// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/network_state_notifier.h"
#endif

// Are we in the process of restoring?
static bool restoring = false;

namespace {

// TabLoader ------------------------------------------------------------------

// Initial delay (see class decription for details).
static const int kInitialDelayTimerMS = 100;

// TabLoader is responsible for loading tabs after session restore creates
// tabs. New tabs are loaded after the current tab finishes loading, or a delay
// is reached (initially kInitialDelayTimerMS). If the delay is reached before
// a tab finishes loading a new tab is loaded and the time of the delay
// doubled. When all tabs are loading TabLoader deletes itself.
//
// This is not part of SessionRestoreImpl so that synchronous destruction
// of SessionRestoreImpl doesn't have timing problems.
class TabLoader : public NotificationObserver {
 public:
  typedef std::list<NavigationController*> TabsToLoad;

  TabLoader();
  ~TabLoader();

  // Schedules a tab for loading.
  void ScheduleLoad(NavigationController* controller);

  // Invokes |LoadNextTab| to load a tab.
  //
  // This must be invoked once to start loading.
  void StartLoading();

 private:
  typedef std::set<NavigationController*> TabsLoading;

  // Loads the next tab. If there are no more tabs to load this deletes itself,
  // otherwise |force_load_timer_| is restarted.
  void LoadNextTab();

  // NotificationObserver method. Removes the specified tab and loads the next
  // tab.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Removes the listeners from the specified tab and removes the tab from
  // the set of tabs to load and list of tabs we're waiting to get a load
  // from.
  void RemoveTab(NavigationController* tab);

  // Invoked from |force_load_timer_|. Doubles |force_load_delay_| and invokes
  // |LoadNextTab| to load the next tab
  void ForceLoadTimerFired();

  NotificationRegistrar registrar_;

  // Current delay before a new tab is loaded. See class description for
  // details.
  int64 force_load_delay_;

  // Has Load been invoked?
  bool loading_;

  // The set of tabs we've initiated loading on. This does NOT include the
  // selected tabs.
  TabsLoading tabs_loading_;

  // The tabs we need to load.
  TabsToLoad tabs_to_load_;

  base::OneShotTimer<TabLoader> force_load_timer_;

  DISALLOW_COPY_AND_ASSIGN(TabLoader);
};

TabLoader::TabLoader()
    : force_load_delay_(kInitialDelayTimerMS),
      loading_(false) {
}

TabLoader::~TabLoader() {
  DCHECK(tabs_to_load_.empty() && tabs_loading_.empty());
}

void TabLoader::ScheduleLoad(NavigationController* controller) {
  if (controller) {
    DCHECK(find(tabs_to_load_.begin(), tabs_to_load_.end(), controller) ==
           tabs_to_load_.end());
    tabs_to_load_.push_back(controller);
    registrar_.Add(this, NotificationType::TAB_CLOSED,
                   Source<NavigationController>(controller));
    registrar_.Add(this, NotificationType::LOAD_STOP,
                   Source<NavigationController>(controller));
  } else {
    // Should never get a NULL tab.
    NOTREACHED();
  }
}

void TabLoader::StartLoading() {
#if defined(OS_CHROMEOS)
  if (chromeos::NetworkStateNotifier::is_connected()) {
    loading_ = true;
    LoadNextTab();
  } else {
    // Start listening to network state notification now.
    registrar_.Add(this, NotificationType::NETWORK_STATE_CHANGED,
                   NotificationService::AllSources());
  }
#else
  loading_ = true;
  LoadNextTab();
#endif
}

void TabLoader::LoadNextTab() {
  if (!tabs_to_load_.empty()) {
    NavigationController* tab = tabs_to_load_.front();
    DCHECK(tab);
    tabs_loading_.insert(tab);
    tabs_to_load_.pop_front();
    tab->LoadIfNecessary();
    if (tab->tab_contents()) {
      int tab_index;
      Browser* browser = Browser::GetBrowserForController(tab, &tab_index);
      if (browser && browser->selected_index() != tab_index) {
        // By default tabs are marked as visible. As only the selected tab is
        // visible we need to explicitly tell non-selected tabs they are hidden.
        // Without this call non-selected tabs are not marked as backgrounded.
        //
        // NOTE: We need to do this here rather than when the tab is added to
        // the Browser as at that time not everything has been created, so that
        // the call would do nothing.
        tab->tab_contents()->WasHidden();
      }
    }
  }

  if (tabs_to_load_.empty()) {
    tabs_loading_.clear();
    delete this;
    return;
  }

  if (force_load_timer_.IsRunning())
    force_load_timer_.Stop();
  force_load_timer_.Start(
      base::TimeDelta::FromMilliseconds(force_load_delay_),
      this, &TabLoader::ForceLoadTimerFired);
}

void TabLoader::Observe(NotificationType type,
                        const NotificationSource& source,
                        const NotificationDetails& details) {
  switch (type.value) {
#if defined(OS_CHROMEOS)
    case NotificationType::NETWORK_STATE_CHANGED: {
      chromeos::NetworkStateDetails* state_details =
          Details<chromeos::NetworkStateDetails>(details).ptr();
      switch (state_details->state()) {
        case chromeos::NetworkStateDetails::CONNECTED:
          if (!loading_) {
            loading_ = true;
            LoadNextTab();
          }
          // Start loading
          break;
        case chromeos::NetworkStateDetails::CONNECTING:
        case chromeos::NetworkStateDetails::DISCONNECTED:
          // Disconnected while loading. Set loading_ false so
          // that it stops trying to load next tab.
          loading_ = false;
          break;
        default:
          NOTREACHED() << "Unknown nework state notification:"
                       << state_details->state();
      }
      break;
    }
#endif
    case NotificationType::TAB_CLOSED:
    case NotificationType::LOAD_STOP: {
      NavigationController* tab = Source<NavigationController>(source).ptr();
      RemoveTab(tab);
      if (loading_) {
        LoadNextTab();
        // WARNING: if there are no more tabs to load, we have been deleted.
      } else if (tabs_to_load_.empty()) {
        tabs_loading_.clear();
        delete this;
        return;
      }
      break;
    }
    default:
      NOTREACHED() << "Unknown notification received:" << type.value;
  }
}

void TabLoader::RemoveTab(NavigationController* tab) {
  registrar_.Remove(this, NotificationType::TAB_CLOSED,
                    Source<NavigationController>(tab));
  registrar_.Remove(this, NotificationType::LOAD_STOP,
                    Source<NavigationController>(tab));

  TabsLoading::iterator i = tabs_loading_.find(tab);
  if (i != tabs_loading_.end())
    tabs_loading_.erase(i);

  TabsToLoad::iterator j =
      find(tabs_to_load_.begin(), tabs_to_load_.end(), tab);
  if (j != tabs_to_load_.end())
    tabs_to_load_.erase(j);
}

void TabLoader::ForceLoadTimerFired() {
  force_load_delay_ *= 2;
  LoadNextTab();
}

// SessionRestoreImpl ---------------------------------------------------------

// SessionRestoreImpl is responsible for fetching the set of tabs to create
// from SessionService. SessionRestoreImpl deletes itself when done.

class SessionRestoreImpl : public NotificationObserver {
 public:
  SessionRestoreImpl(Profile* profile,
                     Browser* browser,
                     bool synchronous,
                     bool clobber_existing_window,
                     bool always_create_tabbed_browser,
                     const std::vector<GURL>& urls_to_open)
      : profile_(profile),
        browser_(browser),
        synchronous_(synchronous),
        clobber_existing_window_(clobber_existing_window),
        always_create_tabbed_browser_(always_create_tabbed_browser),
        urls_to_open_(urls_to_open) {
  }

  void Restore() {
    SessionService* session_service = profile_->GetSessionService();
    DCHECK(session_service);
    SessionService::SessionCallback* callback =
        NewCallback(this, &SessionRestoreImpl::OnGotSession);
    session_service->GetLastSession(&request_consumer_, callback);

    if (synchronous_) {
      bool old_state = MessageLoop::current()->NestableTasksAllowed();
      MessageLoop::current()->SetNestableTasksAllowed(true);
      MessageLoop::current()->Run();
      MessageLoop::current()->SetNestableTasksAllowed(old_state);
      ProcessSessionWindows(&windows_);
      delete this;
      return;
    }

    if (browser_) {
      registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                     Source<Browser>(browser_));
    }
  }

  void RestoreForeignSession(std::vector<SessionWindow*>* windows) {
    tab_loader_.reset(new TabLoader());
    // Create a browser instance to put the restored tabs in.
    bool has_tabbed_browser = false;
    for (std::vector<SessionWindow*>::iterator i = (*windows).begin();
        i != (*windows).end(); ++i) {
      Browser* browser = NULL;
      if (!has_tabbed_browser && (*i)->type == Browser::TYPE_NORMAL)
        has_tabbed_browser = true;
      browser = new Browser(static_cast<Browser::Type>((*i)->type),
          profile_);
      browser->set_override_bounds((*i)->bounds);
      browser->set_maximized_state((*i)->is_maximized ?
          Browser::MAXIMIZED_STATE_MAXIMIZED :
          Browser::MAXIMIZED_STATE_UNMAXIMIZED);
      browser->CreateBrowserWindow();

      // Restore and show the browser.
      const int initial_tab_count = browser->tab_count();
      RestoreTabsToBrowser(*(*i), browser);
      ShowBrowser(browser, initial_tab_count,
          (*i)->selected_tab_index);
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
    }
    FinishedTabCreation(true, has_tabbed_browser);
  }

  ~SessionRestoreImpl() {
    STLDeleteElements(&windows_);
    restoring = false;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::BROWSER_CLOSED:
        delete this;
        return;

      default:
        NOTREACHED();
        break;
    }
  }

 private:
  // Invoked when done with creating all the tabs/browsers.
  //
  // |created_tabbed_browser| indicates whether a tabbed browser was created,
  // or we used an existing tabbed browser.
  //
  // If successful, this begins loading tabs and deletes itself when all tabs
  // have been loaded.
  void FinishedTabCreation(bool succeeded, bool created_tabbed_browser) {
    if (!created_tabbed_browser && always_create_tabbed_browser_) {
      Browser* browser = Browser::Create(profile_);
      if (urls_to_open_.empty()) {
        // No tab browsers were created and no URLs were supplied on the command
        // line. Add an empty URL, which is treated as opening the users home
        // page.
        urls_to_open_.push_back(GURL());
      }
      AppendURLsToBrowser(browser, urls_to_open_);
      browser->window()->Show();
    }

    if (succeeded) {
      DCHECK(tab_loader_.get());
      // TabLoader delets itself when done loading.
      tab_loader_.release()->StartLoading();
    }

    if (!synchronous_) {
      // If we're not synchronous we need to delete ourself.
      // NOTE: we must use DeleteLater here as most likely we're in a callback
      // from the history service which doesn't deal well with deleting the
      // object it is notifying.
      MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    }
  }

  void OnGotSession(SessionService::Handle handle,
                    std::vector<SessionWindow*>* windows) {
    if (synchronous_) {
      // See comment above windows_ as to why we don't process immediately.
      windows_.swap(*windows);
      MessageLoop::current()->Quit();
      return;
    }

    ProcessSessionWindows(windows);
  }

  void ProcessSessionWindows(std::vector<SessionWindow*>* windows) {
    if (windows->empty()) {
      // Restore was unsuccessful.
      FinishedTabCreation(false, false);
      return;
    }

    tab_loader_.reset(new TabLoader());

    Browser* current_browser =
        browser_ ? browser_ : BrowserList::GetLastActive();
    // After the for loop this contains the last TABBED_BROWSER. Is null if no
    // tabbed browsers exist.
    Browser* last_browser = NULL;
    bool has_tabbed_browser = false;
    for (std::vector<SessionWindow*>::iterator i = windows->begin();
         i != windows->end(); ++i) {
      Browser* browser = NULL;
      if (!has_tabbed_browser && (*i)->type == Browser::TYPE_NORMAL)
        has_tabbed_browser = true;
      if (i == windows->begin() && (*i)->type == Browser::TYPE_NORMAL &&
          !clobber_existing_window_) {
        // If there is an open tabbed browser window, use it. Otherwise fall
        // through and create a new one.
        browser = current_browser;
        if (browser && (browser->type() != Browser::TYPE_NORMAL ||
                        browser->profile()->IsOffTheRecord())) {
          browser = NULL;
        }
      }
      if (!browser) {
        browser = new Browser(static_cast<Browser::Type>((*i)->type), profile_);
        browser->set_override_bounds((*i)->bounds);
        browser->set_maximized_state((*i)->is_maximized ?
            Browser::MAXIMIZED_STATE_MAXIMIZED :
            Browser::MAXIMIZED_STATE_UNMAXIMIZED);
        browser->CreateBrowserWindow();
      }
      if ((*i)->type == Browser::TYPE_NORMAL)
        last_browser = browser;
      const int initial_tab_count = browser->tab_count();
      RestoreTabsToBrowser(*(*i), browser);
      ShowBrowser(browser, initial_tab_count, (*i)->selected_tab_index);
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
    }

    // If we're restoring a session as the result of a crash and the session
    // included at least one tabbed browser, then close the browser window
    // that was opened when the user clicked to restore the session.
    if (clobber_existing_window_ && current_browser && has_tabbed_browser &&
        current_browser->type() == Browser::TYPE_NORMAL) {
      current_browser->CloseAllTabs();
    }
    if (last_browser && !urls_to_open_.empty())
      AppendURLsToBrowser(last_browser, urls_to_open_);
    // If last_browser is NULL and urls_to_open_ is non-empty,
    // FinishedTabCreation will create a new TabbedBrowser and add the urls to
    // it.
    FinishedTabCreation(true, has_tabbed_browser);
  }

  void RestoreTabsToBrowser(const SessionWindow& window, Browser* browser) {
    DCHECK(!window.tabs.empty());
    for (std::vector<SessionTab*>::const_iterator i = window.tabs.begin();
         i != window.tabs.end(); ++i) {
      const SessionTab& tab = *(*i);
      DCHECK(!tab.navigations.empty());
      int selected_index = tab.current_navigation_index;
      selected_index = std::max(
          0,
          std::min(selected_index,
                   static_cast<int>(tab.navigations.size() - 1)));
      tab_loader_->ScheduleLoad(
          &browser->AddRestoredTab(tab.navigations,
                                   static_cast<int>(i - window.tabs.begin()),
                                   selected_index,
                                   tab.extension_app_id,
                                   false,
                                   tab.pinned,
                                   true,
                                   NULL)->controller());
    }
  }

  void ShowBrowser(Browser* browser,
                   int initial_tab_count,
                   int selected_session_index) {
    if (browser_ == browser) {
      browser->SelectTabContentsAt(browser->tab_count() - 1, true);
      return;
    }

    DCHECK(browser);
    DCHECK(browser->tab_count());
    browser->SelectTabContentsAt(
        std::min(initial_tab_count + std::max(0, selected_session_index),
                 browser->tab_count() - 1), true);
    browser->window()->Show();
    // TODO(jcampan): http://crbug.com/8123 we should not need to set the
    //                initial focus explicitly.
    browser->GetSelectedTabContents()->view()->SetInitialFocus();
  }

  // Appends the urls in |urls| to |browser|.
  void AppendURLsToBrowser(Browser* browser,
                           const std::vector<GURL>& urls) {
    for (size_t i = 0; i < urls.size(); ++i) {
      int add_types = TabStripModel::ADD_FORCE_INDEX;
      if (i == 0)
        add_types |= TabStripModel::ADD_SELECTED;
      int index = browser->GetIndexForInsertionDuringRestore(i);
      browser::NavigateParams params(browser, urls[i],
                                     PageTransition::START_PAGE);
      params.disposition = i == 0 ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
      params.tabstrip_index = index;
      params.tabstrip_add_types = add_types;
      browser::Navigate(&params);
    }
  }

  // Invokes TabRestored on the SessionService for all tabs in browser after
  // initial_count.
  void NotifySessionServiceOfRestoredTabs(Browser* browser, int initial_count) {
    SessionService* session_service = profile_->GetSessionService();
    for (int i = initial_count; i < browser->tab_count(); ++i)
      session_service->TabRestored(&browser->GetTabContentsAt(i)->controller(),
                                   browser->tabstrip_model()->IsTabPinned(i));
  }

  // The profile to create the sessions for.
  Profile* profile_;

  // The first browser to restore to, may be null.
  Browser* browser_;

  // Whether or not restore is synchronous.
  const bool synchronous_;

  // See description in RestoreSession (in .h).
  const bool clobber_existing_window_;

  // If true and there is an error or there are no windows to restore, we
  // create a tabbed browser anyway. This is used on startup to make sure at
  // at least one window is created.
  const bool always_create_tabbed_browser_;

  // Set of URLs to open in addition to those restored from the session.
  std::vector<GURL> urls_to_open_;

  // Used to get the session.
  CancelableRequestConsumer request_consumer_;

  // Responsible for loading the tabs.
  scoped_ptr<TabLoader> tab_loader_;

  // When synchronous we run a nested message loop. To avoid creating windows
  // from the nested message loop (which can make exiting the nested message
  // loop take a while) we cache the SessionWindows here and create the actual
  // windows when the nested message loop exits.
  std::vector<SessionWindow*> windows_;

  NotificationRegistrar registrar_;
};

}  // namespace

// SessionRestore -------------------------------------------------------------

static void Restore(Profile* profile,
                    Browser* browser,
                    bool synchronous,
                    bool clobber_existing_window,
                    bool always_create_tabbed_browser,
                    const std::vector<GURL>& urls_to_open) {
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "SessionRestoreStarted", false);
#endif
  DCHECK(profile);
  // Always restore from the original profile (incognito profiles have no
  // session service).
  profile = profile->GetOriginalProfile();
  if (!profile->GetSessionService()) {
    NOTREACHED();
    return;
  }
  restoring = true;
  profile->set_restored_last_session(true);
  // SessionRestoreImpl takes care of deleting itself when done.
  SessionRestoreImpl* restorer =
      new SessionRestoreImpl(profile, browser, synchronous,
                             clobber_existing_window,
                             always_create_tabbed_browser, urls_to_open);
  restorer->Restore();
}

// static
void SessionRestore::RestoreSession(Profile* profile,
                                    Browser* browser,
                                    bool clobber_existing_window,
                                    bool always_create_tabbed_browser,
                                    const std::vector<GURL>& urls_to_open) {
  Restore(profile, browser, false, clobber_existing_window,
          always_create_tabbed_browser, urls_to_open);
}

// static
void SessionRestore::RestoreForeignSessionWindows(Profile* profile,
    std::vector<SessionWindow*>* windows) {
  // Create a SessionRestore object to eventually restore the tabs.
  std::vector<GURL> gurls;
  SessionRestoreImpl restorer(profile,
      static_cast<Browser*>(NULL), true, false, true, gurls);
  restorer.RestoreForeignSession(windows);
}

// static
void SessionRestore::RestoreSessionSynchronously(
    Profile* profile,
    const std::vector<GURL>& urls_to_open) {
  Restore(profile, NULL, true, false, true, urls_to_open);
}

// static
bool SessionRestore::IsRestoring() {
  return restoring;
}
