// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include <algorithm>
#include <list>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/performance_monitor/startup_timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/base/network_change_notifier.h"
#include "webkit/glue/glue_serialize.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
#endif

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif

#if defined(USE_ASH)
#include "ash/wm/window_util.h"
#endif
using content::NavigationController;
using content::RenderWidgetHost;
using content::WebContents;

namespace {

class SessionRestoreImpl;
class TabLoader;

TabLoader* shared_tab_loader = NULL;

// Pointers to SessionRestoreImpls which are currently restoring the session.
std::set<SessionRestoreImpl*>* active_session_restorers = NULL;

// TabLoader ------------------------------------------------------------------

// Initial delay (see class decription for details).
static const int kInitialDelayTimerMS = 100;

// TabLoader is responsible for loading tabs after session restore creates
// tabs. New tabs are loaded after the current tab finishes loading, or a delay
// is reached (initially kInitialDelayTimerMS). If the delay is reached before
// a tab finishes loading a new tab is loaded and the time of the delay
// doubled.
//
// TabLoader keeps a reference to itself when it's loading. When it has finished
// loading, it drops the reference. If another profile is restored while the
// TabLoader is loading, it will schedule its tabs to get loaded by the same
// TabLoader. When doing the scheduling, it holds a reference to the TabLoader.
//
// This is not part of SessionRestoreImpl so that synchronous destruction
// of SessionRestoreImpl doesn't have timing problems.
class TabLoader : public content::NotificationObserver,
                  public net::NetworkChangeNotifier::ConnectionTypeObserver,
                  public base::RefCounted<TabLoader> {
 public:
  // Retrieves a pointer to the TabLoader instance shared between profiles, or
  // creates a new TabLoader if it doesn't exist. If a TabLoader is created, its
  // starting timestamp is set to |restore_started|.
  static TabLoader* GetTabLoader(base::TimeTicks restore_started);

  // Schedules a tab for loading.
  void ScheduleLoad(NavigationController* controller);

  // Notifies the loader that a tab has been scheduled for loading through
  // some other mechanism.
  void TabIsLoading(NavigationController* controller);

  // Invokes |LoadNextTab| to load a tab.
  //
  // This must be invoked once to start loading.
  void StartLoading();

 private:
  friend class base::RefCounted<TabLoader>;

  typedef std::set<NavigationController*> TabsLoading;
  typedef std::list<NavigationController*> TabsToLoad;
  typedef std::set<RenderWidgetHost*> RenderWidgetHostSet;

  explicit TabLoader(base::TimeTicks restore_started);
  virtual ~TabLoader();

  // Loads the next tab. If there are no more tabs to load this deletes itself,
  // otherwise |force_load_timer_| is restarted.
  void LoadNextTab();

  // NotificationObserver method. Removes the specified tab and loads the next
  // tab.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // net::NetworkChangeNotifier::ConnectionTypeObserver overrides.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // Removes the listeners from the specified tab and removes the tab from
  // the set of tabs to load and list of tabs we're waiting to get a load
  // from.
  void RemoveTab(NavigationController* tab);

  // Invoked from |force_load_timer_|. Doubles |force_load_delay_| and invokes
  // |LoadNextTab| to load the next tab
  void ForceLoadTimerFired();

  // Returns the RenderWidgetHost associated with a tab if there is one,
  // NULL otherwise.
  static RenderWidgetHost* GetRenderWidgetHost(NavigationController* tab);

  // Register for necessary notifications on a tab navigation controller.
  void RegisterForNotifications(NavigationController* controller);

  // Called when a tab goes away or a load completes.
  void HandleTabClosedOrLoaded(NavigationController* controller);

  content::NotificationRegistrar registrar_;

  // Current delay before a new tab is loaded. See class description for
  // details.
  int64 force_load_delay_;

  // Has Load been invoked?
  bool loading_;

  // Have we recorded the times for a tab paint?
  bool got_first_paint_;

  // The set of tabs we've initiated loading on. This does NOT include the
  // selected tabs.
  TabsLoading tabs_loading_;

  // The tabs we need to load.
  TabsToLoad tabs_to_load_;

  // The renderers we have started loading into.
  RenderWidgetHostSet render_widget_hosts_loading_;

  // The renderers we have loaded and are waiting on to paint.
  RenderWidgetHostSet render_widget_hosts_to_paint_;

  // The number of tabs that have been restored.
  int tab_count_;

  base::OneShotTimer<TabLoader> force_load_timer_;

  // The time the restore process started.
  base::TimeTicks restore_started_;

  // Max number of tabs that were loaded in parallel (for metrics).
  size_t max_parallel_tab_loads_;

  // For keeping TabLoader alive while it's loading even if no
  // SessionRestoreImpls reference it.
  scoped_refptr<TabLoader> this_retainer_;

  DISALLOW_COPY_AND_ASSIGN(TabLoader);
};

// static
TabLoader* TabLoader::GetTabLoader(base::TimeTicks restore_started) {
  if (!shared_tab_loader)
    shared_tab_loader = new TabLoader(restore_started);
  return shared_tab_loader;
}

void TabLoader::ScheduleLoad(NavigationController* controller) {
  DCHECK(controller);
  DCHECK(find(tabs_to_load_.begin(), tabs_to_load_.end(), controller) ==
         tabs_to_load_.end());
  tabs_to_load_.push_back(controller);
  RegisterForNotifications(controller);
}

void TabLoader::TabIsLoading(NavigationController* controller) {
  DCHECK(controller);
  DCHECK(find(tabs_loading_.begin(), tabs_loading_.end(), controller) ==
         tabs_loading_.end());
  tabs_loading_.insert(controller);
  RenderWidgetHost* render_widget_host = GetRenderWidgetHost(controller);
  DCHECK(render_widget_host);
  render_widget_hosts_loading_.insert(render_widget_host);
  RegisterForNotifications(controller);
}

void TabLoader::StartLoading() {
  // When multiple profiles are using the same TabLoader, another profile might
  // already have started loading. In that case, the tabs scheduled for loading
  // by this profile are already in the loading queue, and they will get loaded
  // eventually.
  if (loading_)
    return;
  registrar_.Add(
      this,
      content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      content::NotificationService::AllSources());
  this_retainer_ = this;
#if defined(OS_CHROMEOS)
  if (!net::NetworkChangeNotifier::IsOffline()) {
    loading_ = true;
    LoadNextTab();
  } else {
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  }
#else
  loading_ = true;
  LoadNextTab();
#endif
}

TabLoader::TabLoader(base::TimeTicks restore_started)
    : force_load_delay_(kInitialDelayTimerMS),
      loading_(false),
      got_first_paint_(false),
      tab_count_(0),
      restore_started_(restore_started),
      max_parallel_tab_loads_(0) {
}

TabLoader::~TabLoader() {
  DCHECK((got_first_paint_ || render_widget_hosts_to_paint_.empty()) &&
          tabs_loading_.empty() && tabs_to_load_.empty());
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  shared_tab_loader = NULL;
}

void TabLoader::LoadNextTab() {
  if (!tabs_to_load_.empty()) {
    NavigationController* tab = tabs_to_load_.front();
    DCHECK(tab);
    tabs_loading_.insert(tab);
    if (tabs_loading_.size() > max_parallel_tab_loads_)
      max_parallel_tab_loads_ = tabs_loading_.size();
    tabs_to_load_.pop_front();
    tab->LoadIfNecessary();
    content::WebContents* contents = tab->GetWebContents();
    if (contents) {
      Browser* browser = chrome::FindBrowserWithWebContents(contents);
      if (browser &&
          browser->tab_strip_model()->GetActiveWebContents() != contents) {
        // By default tabs are marked as visible. As only the active tab is
        // visible we need to explicitly tell non-active tabs they are hidden.
        // Without this call non-active tabs are not marked as backgrounded.
        //
        // NOTE: We need to do this here rather than when the tab is added to
        // the Browser as at that time not everything has been created, so that
        // the call would do nothing.
        contents->WasHidden();
      }
    }
  }

  if (!tabs_to_load_.empty()) {
    force_load_timer_.Stop();
    // Each time we load a tab we also set a timer to force us to start loading
    // the next tab if this one doesn't load quickly enough.
    force_load_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(force_load_delay_),
        this, &TabLoader::ForceLoadTimerFired);
  }
}

void TabLoader::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_START: {
      // Add this render_widget_host to the set of those we're waiting for
      // paints on. We want to only record stats for paints that occur after
      // a load has finished.
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      RenderWidgetHost* render_widget_host = GetRenderWidgetHost(tab);
      DCHECK(render_widget_host);
      render_widget_hosts_loading_.insert(render_widget_host);
      break;
    }
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      if (!got_first_paint_) {
        RenderWidgetHost* render_widget_host =
            GetRenderWidgetHost(&web_contents->GetController());
        render_widget_hosts_loading_.erase(render_widget_host);
      }
      HandleTabClosedOrLoaded(&web_contents->GetController());
      break;
    }
    case content::NOTIFICATION_LOAD_STOP: {
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      render_widget_hosts_to_paint_.insert(GetRenderWidgetHost(tab));
      HandleTabClosedOrLoaded(tab);
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE: {
      RenderWidgetHost* render_widget_host =
          content::Source<RenderWidgetHost>(source).ptr();
      if (!got_first_paint_ && render_widget_host->GetView() &&
          render_widget_host->GetView()->IsShowing()) {
        if (render_widget_hosts_to_paint_.find(render_widget_host) !=
            render_widget_hosts_to_paint_.end()) {
          // Got a paint for one of our renderers, so record time.
          got_first_paint_ = true;
          base::TimeDelta time_to_paint =
              base::TimeTicks::Now() - restore_started_;
          UMA_HISTOGRAM_CUSTOM_TIMES(
              "SessionRestore.FirstTabPainted",
              time_to_paint,
              base::TimeDelta::FromMilliseconds(10),
              base::TimeDelta::FromSeconds(100),
              100);
          // Record a time for the number of tabs, to help track down
          // contention.
          std::string time_for_count =
              base::StringPrintf("SessionRestore.FirstTabPainted_%d",
                                 tab_count_);
          base::Histogram* counter_for_count =
              base::Histogram::FactoryTimeGet(
                  time_for_count,
                  base::TimeDelta::FromMilliseconds(10),
                  base::TimeDelta::FromSeconds(100),
                  100,
                  base::Histogram::kUmaTargetedHistogramFlag);
          counter_for_count->AddTime(time_to_paint);
        } else if (render_widget_hosts_loading_.find(render_widget_host) ==
            render_widget_hosts_loading_.end()) {
          // If this is a host for a tab we're not loading some other tab
          // has rendered and there's no point tracking the time. This could
          // happen because the user opened a different tab or restored tabs
          // to an already existing browser and an existing tab painted.
          got_first_paint_ = true;
        }
      }
      break;
    }
    default:
      NOTREACHED() << "Unknown notification received:" << type;
  }
  // Delete ourselves when we're not waiting for any more notifications. If this
  // was not the last reference, a SessionRestoreImpl holding a reference will
  // eventually call StartLoading (which assigns this_retainer_), or drop the
  // reference without initiating a load.
  if ((got_first_paint_ || render_widget_hosts_to_paint_.empty()) &&
      tabs_loading_.empty() && tabs_to_load_.empty())
    this_retainer_ = NULL;
}

void TabLoader::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE) {
    if (!loading_) {
      loading_ = true;
      LoadNextTab();
    }
  } else {
    loading_ = false;
  }
}

void TabLoader::RemoveTab(NavigationController* tab) {
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<WebContents>(tab->GetWebContents()));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_STOP,
                    content::Source<NavigationController>(tab));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_START,
                    content::Source<NavigationController>(tab));

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

RenderWidgetHost* TabLoader::GetRenderWidgetHost(NavigationController* tab) {
  WebContents* web_contents = tab->GetWebContents();
  if (web_contents) {
    content::RenderWidgetHostView* render_widget_host_view =
        web_contents->GetRenderWidgetHostView();
    if (render_widget_host_view)
      return render_widget_host_view->GetRenderWidgetHost();
  }
  return NULL;
}

void TabLoader::RegisterForNotifications(NavigationController* controller) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(controller->GetWebContents()));
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::Source<NavigationController>(controller));
  ++tab_count_;
}

void TabLoader::HandleTabClosedOrLoaded(NavigationController* tab) {
  RemoveTab(tab);
  if (loading_)
    LoadNextTab();
  if (tabs_loading_.empty() && tabs_to_load_.empty()) {
    base::TimeDelta time_to_load =
        base::TimeTicks::Now() - restore_started_;
    performance_monitor::StartupTimer::SetElapsedSessionRestoreTime(
        time_to_load);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "SessionRestore.AllTabsLoaded",
        time_to_load,
        base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromSeconds(100),
        100);
    // Record a time for the number of tabs, to help track down contention.
    std::string time_for_count =
        base::StringPrintf("SessionRestore.AllTabsLoaded_%d", tab_count_);
    base::Histogram* counter_for_count =
        base::Histogram::FactoryTimeGet(
            time_for_count,
            base::TimeDelta::FromMilliseconds(10),
            base::TimeDelta::FromSeconds(100),
            100,
            base::Histogram::kUmaTargetedHistogramFlag);
    counter_for_count->AddTime(time_to_load);

    UMA_HISTOGRAM_COUNTS_100("SessionRestore.ParallelTabLoads",
                             max_parallel_tab_loads_);
  }
}

// SessionRestoreImpl ---------------------------------------------------------

// SessionRestoreImpl is responsible for fetching the set of tabs to create
// from SessionService. SessionRestoreImpl deletes itself when done.

class SessionRestoreImpl : public content::NotificationObserver {
 public:
  SessionRestoreImpl(Profile* profile,
                     Browser* browser,
                     chrome::HostDesktopType host_desktop_type,
                     bool synchronous,
                     bool clobber_existing_tab,
                     bool always_create_tabbed_browser,
                     const std::vector<GURL>& urls_to_open)
      : profile_(profile),
        browser_(browser),
        host_desktop_type_(host_desktop_type),
        synchronous_(synchronous),
        clobber_existing_tab_(clobber_existing_tab),
        always_create_tabbed_browser_(always_create_tabbed_browser),
        urls_to_open_(urls_to_open),
        active_window_id_(0),
        restore_started_(base::TimeTicks::Now()),
        browser_shown_(false) {
    // For sanity's sake, if |browser| is non-null: force |host_desktop_type| to
    // be the same as |browser|'s desktop type.
    DCHECK(!browser || browser->host_desktop_type() == host_desktop_type);

    if (active_session_restorers == NULL)
      active_session_restorers = new std::set<SessionRestoreImpl*>();

    // Only one SessionRestoreImpl should be operating on the profile at the
    // same time.
    std::set<SessionRestoreImpl*>::const_iterator it;
    for (it = active_session_restorers->begin();
         it != active_session_restorers->end(); ++it) {
      if ((*it)->profile_ == profile)
        break;
    }
    DCHECK(it == active_session_restorers->end());

    active_session_restorers->insert(this);

    // When asynchronous its possible for there to be no windows. To make sure
    // Chrome doesn't prematurely exit AddRef the process. We'll release in the
    // destructor when restore is done.
    g_browser_process->AddRefModule();
  }

  Browser* Restore() {
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile_);
    DCHECK(session_service);
    session_service->GetLastSession(
        base::Bind(&SessionRestoreImpl::OnGotSession, base::Unretained(this)),
        &cancelable_task_tracker_);

    if (synchronous_) {
      {
        MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
        MessageLoop::current()->Run();
      }
      Browser* browser = ProcessSessionWindows(&windows_, active_window_id_);
      delete this;
      return browser;
    }

    if (browser_) {
      registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                     content::Source<Browser>(browser_));
    }

    return browser_;
  }

  // Restore window(s) from a foreign session.
  void RestoreForeignSession(
      std::vector<const SessionWindow*>::const_iterator begin,
      std::vector<const SessionWindow*>::const_iterator end) {
    StartTabCreation();
    // Create a browser instance to put the restored tabs in.
    for (std::vector<const SessionWindow*>::const_iterator i = begin;
         i != end; ++i) {
      Browser* browser = CreateRestoredBrowser(
          static_cast<Browser::Type>((*i)->type),
          (*i)->bounds,
          (*i)->show_state,
          (*i)->app_name);

      // Restore and show the browser.
      const int initial_tab_count = 0;
      int selected_tab_index = std::max(
          0,
          std::min((*i)->selected_tab_index,
                   static_cast<int>((*i)->tabs.size()) - 1));
      selected_tab_index =
          RestoreTabsToBrowser(*(*i), browser, selected_tab_index);
      ShowBrowser(browser, selected_tab_index);
      tab_loader_->TabIsLoading(
          &browser->tab_strip_model()->GetActiveWebContents()->GetController());
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
    }

    // Always create in a new window
    FinishedTabCreation(true, true);
  }

  // Restore a single tab from a foreign session.
  // Opens in the tab in the last active browser, unless disposition is
  // NEW_WINDOW, in which case the tab will be opened in a new browser.
  void RestoreForeignTab(const SessionTab& tab,
                         WindowOpenDisposition disposition) {
    DCHECK(!tab.navigations.empty());
    int selected_index = tab.current_navigation_index;
    selected_index = std::max(
        0,
        std::min(selected_index,
                 static_cast<int>(tab.navigations.size() - 1)));

    bool use_new_window = disposition == NEW_WINDOW;

    Browser* browser = use_new_window ?
        new Browser(Browser::CreateParams(profile_, host_desktop_type_)) :
        browser_;

    RecordAppLaunchForTab(browser, tab, selected_index);

    if (disposition == CURRENT_TAB) {
      DCHECK(!use_new_window);
      chrome::ReplaceRestoredTab(browser,
                                 tab.navigations,
                                 selected_index,
                                 true,
                                 tab.extension_app_id,
                                 NULL,
                                 tab.user_agent_override);
    } else {
      int tab_index =
          use_new_window ? 0 : browser->tab_strip_model()->active_index() + 1;
      WebContents* web_contents = chrome::AddRestoredTab(
          browser,
          tab.navigations,
          tab_index,
          selected_index,
          tab.extension_app_id,
          disposition == NEW_FOREGROUND_TAB,  // selected
          tab.pinned,
          true,
          NULL,
          tab.user_agent_override);
      // Start loading the tab immediately.
      web_contents->GetController().LoadIfNecessary();
    }

    if (use_new_window) {
      browser->tab_strip_model()->ActivateTabAt(0, true);
      browser->window()->Show();
    }
    NotifySessionServiceOfRestoredTabs(browser,
                                       browser->tab_strip_model()->count());

    // Since FinishedTabCreation() is not called here, |this| will leak if we
    // are not in sychronous mode.
    DCHECK(synchronous_);
  }

  ~SessionRestoreImpl() {
    STLDeleteElements(&windows_);

    active_session_restorers->erase(this);
    if (active_session_restorers->empty()) {
      delete active_session_restorers;
      active_session_restorers = NULL;
    }

    g_browser_process->ReleaseModule();
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    switch (type) {
      case chrome::NOTIFICATION_BROWSER_CLOSED:
        delete this;
        return;

      default:
        NOTREACHED();
        break;
    }
  }

  Profile* profile() { return profile_; }

 private:
  // Invoked when beginning to create new tabs. Resets the tab_loader_.
  void StartTabCreation() {
    tab_loader_ = TabLoader::GetTabLoader(restore_started_);
  }

  // Invoked when done with creating all the tabs/browsers.
  //
  // |created_tabbed_browser| indicates whether a tabbed browser was created,
  // or we used an existing tabbed browser.
  //
  // If successful, this begins loading tabs and deletes itself when all tabs
  // have been loaded.
  //
  // Returns the Browser that was created, if any.
  Browser* FinishedTabCreation(bool succeeded, bool created_tabbed_browser) {
    Browser* browser = NULL;
    if (!created_tabbed_browser && always_create_tabbed_browser_) {
      browser = new Browser(Browser::CreateParams(profile_,
                                                  host_desktop_type_));
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
      // TabLoader deletes itself when done loading.
      tab_loader_->StartLoading();
      tab_loader_ = NULL;
    }

    if (!synchronous_) {
      // If we're not synchronous we need to delete ourself.
      // NOTE: we must use DeleteLater here as most likely we're in a callback
      // from the history service which doesn't deal well with deleting the
      // object it is notifying.
      MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    }

#if defined(OS_CHROMEOS)
    chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
        "SessionRestore-End", false);
#endif
    return browser;
  }

  void OnGotSession(ScopedVector<SessionWindow> windows,
                    SessionID::id_type active_window_id) {
    base::TimeDelta time_to_got_sessions =
        base::TimeTicks::Now() - restore_started_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "SessionRestore.TimeToGotSessions",
        time_to_got_sessions,
        base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromSeconds(1000),
        100);
#if defined(OS_CHROMEOS)
    chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
        "SessionRestore-GotSession", false);
#endif
    if (synchronous_) {
      // See comment above windows_ as to why we don't process immediately.
      windows_.swap(windows.get());
      active_window_id_ = active_window_id;
      MessageLoop::current()->QuitNow();
      return;
    }

    ProcessSessionWindows(&windows.get(), active_window_id);
  }

  Browser* ProcessSessionWindows(std::vector<SessionWindow*>* windows,
                                 SessionID::id_type active_window_id) {
    VLOG(1) << "ProcessSessionWindows " << windows->size();
    base::TimeDelta time_to_process_sessions =
        base::TimeTicks::Now() - restore_started_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "SessionRestore.TimeToProcessSessions",
        time_to_process_sessions,
        base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromSeconds(1000),
        100);

    if (windows->empty()) {
      // Restore was unsuccessful. The DOM storage system can also delete its
      // data, since no session restore will happen at a later point in time.
      content::BrowserContext::GetDefaultStoragePartition(profile_)->
          GetDOMStorageContext()->StartScavengingUnusedSessionStorage();
      return FinishedTabCreation(false, false);
    }

#if defined(OS_CHROMEOS)
    chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
        "SessionRestore-CreatingTabs-Start", false);
#endif
    StartTabCreation();

    // After the for loop this contains the last TABBED_BROWSER. Is null if no
    // tabbed browsers exist.
    Browser* last_browser = NULL;
    bool has_tabbed_browser = false;

    // After the for loop, this contains the browser to activate, if one of the
    // windows has the same id as specified in active_window_id.
    Browser* browser_to_activate = NULL;
    int selected_tab_to_activate = -1;

    // Determine if there is a visible window.
    bool has_visible_browser = false;
    for (std::vector<SessionWindow*>::iterator i = windows->begin();
         i != windows->end(); ++i) {
      if ((*i)->show_state != ui::SHOW_STATE_MINIMIZED)
        has_visible_browser = true;
    }

    for (std::vector<SessionWindow*>::iterator i = windows->begin();
         i != windows->end(); ++i) {
      Browser* browser = NULL;
      if (!has_tabbed_browser && (*i)->type == Browser::TYPE_TABBED)
        has_tabbed_browser = true;
      if (i == windows->begin() && (*i)->type == Browser::TYPE_TABBED &&
          browser_ && browser_->is_type_tabbed() &&
          !browser_->profile()->IsOffTheRecord()) {
        // The first set of tabs is added to the existing browser.
        browser = browser_;
      } else {
#if defined(OS_CHROMEOS)
    chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
        "SessionRestore-CreateRestoredBrowser-Start", false);
#endif
        // Show the first window if none are visible.
        ui::WindowShowState show_state = (*i)->show_state;
        if (!has_visible_browser) {
          show_state = ui::SHOW_STATE_NORMAL;
          has_visible_browser = true;
        }
        browser = NULL;
#if defined(OS_WIN)
        if (win8::IsSingleWindowMetroMode()) {
          // We don't want to add tabs to the off the record browser.
          if (browser_ && !browser_->profile()->IsOffTheRecord()) {
            browser = browser_;
          } else {
            browser = last_browser;
            // last_browser should never be off the record either.
            // We don't set browser higher above when browser_ is offtherecord,
            // and CreateRestoredBrowser below, is never created offtherecord.
            DCHECK(!browser || !browser->profile()->IsOffTheRecord());
          }
          // Metro should only have tabbed browsers.
          // It never creates any non-tabbed browser, and thus should never
          // restore non-tabbed items...
          DCHECK(!browser || browser->is_type_tabbed());
          DCHECK((*i)->type == Browser::TYPE_TABBED);
        }
#endif
        if (!browser) {
          browser = CreateRestoredBrowser(
              static_cast<Browser::Type>((*i)->type),
              (*i)->bounds,
              show_state,
              (*i)->app_name);
        }
#if defined(OS_CHROMEOS)
    chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
        "SessionRestore-CreateRestoredBrowser-End", false);
#endif
      }
      if ((*i)->type == Browser::TYPE_TABBED)
        last_browser = browser;
      WebContents* active_tab =
          browser->tab_strip_model()->GetActiveWebContents();
      int initial_tab_count = browser->tab_strip_model()->count();
      int selected_tab_index = std::max(
          0,
          std::min((*i)->selected_tab_index,
                   static_cast<int>((*i)->tabs.size()) - 1));
      selected_tab_index =
          RestoreTabsToBrowser(*(*i), browser, selected_tab_index);
      ShowBrowser(browser, selected_tab_index);
      if ((*i)->window_id.id() == active_window_id) {
        browser_to_activate = browser;
        selected_tab_to_activate = selected_tab_index;
      }
      if (clobber_existing_tab_ && i == windows->begin() &&
          (*i)->type == Browser::TYPE_TABBED && active_tab &&
          browser == browser_ &&
          browser->tab_strip_model()->count() > initial_tab_count) {
        chrome::CloseWebContents(browser, active_tab, true);
        selected_tab_to_activate = -1;
      }
      tab_loader_->TabIsLoading(
          &browser->tab_strip_model()->GetActiveWebContents()->GetController());
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
    }

    if (browser_to_activate && browser_to_activate->is_type_tabbed())
      last_browser = browser_to_activate;

    if (last_browser && !urls_to_open_.empty())
      AppendURLsToBrowser(last_browser, urls_to_open_);
#if defined(OS_CHROMEOS)
    chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
        "SessionRestore-CreatingTabs-End", false);
#endif
    if (browser_to_activate) {
      browser_to_activate->window()->Activate();
      // On Win8 Metro, we merge all browsers together, so if we need to
      // activate one of the previously separated window, we need to activate
      // the tab. But we don't keep this within IsMetro, so that other platforms
      // don't complain about an unused variable. Also, selected_tab_to_activate
      // can be -1 if we clobbered the tab that would have been activated.
      // In that case we'll leave activation to last tab.
      // The only current usage of clobber is for crash recovery, so it's fine.
      if (selected_tab_to_activate != -1)
        ShowBrowser(browser_to_activate, selected_tab_to_activate);
    }

    // If last_browser is NULL and urls_to_open_ is non-empty,
    // FinishedTabCreation will create a new TabbedBrowser and add the urls to
    // it.
    Browser* finished_browser = FinishedTabCreation(true, has_tabbed_browser);
    if (finished_browser)
      last_browser = finished_browser;

    // sessionStorages needed for the session restore have now been recreated
    // by RestoreTab. Now it's safe for the DOM storage system to start
    // deleting leftover data.
    content::BrowserContext::GetDefaultStoragePartition(profile_)->
        GetDOMStorageContext()->StartScavengingUnusedSessionStorage();
    return last_browser;
  }

  // Record an app launch event (if appropriate) for a tab which is about to
  // be restored. Callers should ensure that selected_index is within the
  // bounds of tab.navigations before calling.
  void RecordAppLaunchForTab(Browser* browser,
                             const SessionTab& tab,
                             int selected_index) {
    DCHECK(selected_index >= 0 &&
           selected_index < static_cast<int>(tab.navigations.size()));
    GURL url = tab.navigations[selected_index].virtual_url();
    if (browser->profile()->GetExtensionService() &&
        browser->profile()->GetExtensionService()->IsInstalledApp(url)) {
      AppLauncherHandler::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_SESSION_RESTORE);
    }
  }

  // Adds the tabs from |window| to |browser|. Returns the final index of the
  // selected tab.
  int RestoreTabsToBrowser(const SessionWindow& window,
                           Browser* browser,
                           int selected_tab_index) {
    VLOG(1) << "RestoreTabsToBrowser " << window.tabs.size();
    DCHECK(!window.tabs.empty());
    WebContents* selected_web_contents = NULL;
    // If browser already has tabs, we want to restore the new ones after the
    // existing ones. E.g., this happens in Win8 Metro where we merge windows.
    int tab_index_offset = browser->tab_strip_model()->count();
    for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
      const SessionTab& tab = *(window.tabs[i]);
      // Don't schedule a load for the selected tab, as ShowBrowser() will do
      // that.
      if (i == selected_tab_index) {
        selected_web_contents = RestoreTab(
            tab, tab_index_offset + i, browser, false);
      } else {
        RestoreTab(tab, tab_index_offset + i, browser, true);
      }
    }
    if (selected_web_contents) {
      return browser->tab_strip_model()->
          GetIndexOfWebContents(selected_web_contents);
    }
    return 0;
  }

  WebContents* RestoreTab(const SessionTab& tab,
                          const int tab_index,
                          Browser* browser,
                          bool schedule_load) {
    // It's possible (particularly for foreign sessions) to receive a tab
    // without valid navigations. In that case, just skip it.
    // See crbug.com/154129.
    if (tab.navigations.empty())
      return NULL;
    int selected_index = tab.current_navigation_index;
    selected_index = std::max(
        0,
        std::min(selected_index,
                 static_cast<int>(tab.navigations.size() - 1)));

    RecordAppLaunchForTab(browser, tab, selected_index);

    // Associate sessionStorage (if any) to the restored tab.
    scoped_refptr<content::SessionStorageNamespace> session_storage_namespace;
    if (!tab.session_storage_persistent_id.empty()) {
      session_storage_namespace =
          content::BrowserContext::GetDefaultStoragePartition(profile_)->
              GetDOMStorageContext()->RecreateSessionStorage(
                  tab.session_storage_persistent_id);
    }

    WebContents* web_contents =
        chrome::AddRestoredTab(browser,
                               tab.navigations,
                               tab_index,
                               selected_index,
                               tab.extension_app_id,
                               false,  // select
                               tab.pinned,
                               true,
                               session_storage_namespace.get(),
                               tab.user_agent_override);
    // Regression check: check that the tab didn't start loading right away. The
    // focused tab will be loaded by Browser, and TabLoader will load the rest.
    DCHECK(web_contents->GetController().NeedsReload());

    // Set up the file access rights for the selected navigation entry.
    const int id = web_contents->GetRenderProcessHost()->GetID();
    const int read_file_permissions =
        base::PLATFORM_FILE_OPEN |
        base::PLATFORM_FILE_READ |
        base::PLATFORM_FILE_EXCLUSIVE_READ |
        base::PLATFORM_FILE_ASYNC;
    const std::string& state =
        tab.navigations.at(selected_index).content_state();
    const std::vector<FilePath>& file_paths =
        webkit_glue::FilePathsFromHistoryState(state);
    for (std::vector<FilePath>::const_iterator file = file_paths.begin();
         file != file_paths.end(); ++file) {
      content::ChildProcessSecurityPolicy::GetInstance()->
          GrantPermissionsForFile(id, *file, read_file_permissions);
    }

    if (schedule_load)
      tab_loader_->ScheduleLoad(&web_contents->GetController());
    return web_contents;
  }

  Browser* CreateRestoredBrowser(Browser::Type type,
                                 gfx::Rect bounds,
                                 ui::WindowShowState show_state,
                                 const std::string& app_name) {
    Browser::CreateParams params(type, profile_, host_desktop_type_);
    params.app_name = app_name;
    params.initial_bounds = bounds;
    params.initial_show_state = show_state;
    params.is_session_restore = true;
    return new Browser(params);
  }

  void ShowBrowser(Browser* browser, int selected_tab_index) {
    DCHECK(browser);
    DCHECK(browser->tab_strip_model()->count());
    browser->tab_strip_model()->ActivateTabAt(selected_tab_index, true);

    if (browser_ == browser)
      return;

#if defined(USE_ASH)
    // Prevent the auto window management for this window on show.
    ash::wm::SetUserHasChangedWindowPositionOrSize(
        browser->window()->GetNativeWindow(), true);
#endif
    browser->window()->Show();
#if defined(USE_ASH)
    ash::wm::SetUserHasChangedWindowPositionOrSize(
        browser->window()->GetNativeWindow(), false);
#endif
    browser->set_is_session_restore(false);

    // TODO(jcampan): http://crbug.com/8123 we should not need to set the
    //                initial focus explicitly.
    browser->tab_strip_model()->GetActiveWebContents()->
        GetView()->SetInitialFocus();

    if (!browser_shown_) {
      browser_shown_ = true;
      base::TimeDelta time_to_first_show =
          base::TimeTicks::Now() - restore_started_;
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "SessionRestore.TimeToFirstShow",
          time_to_first_show,
          base::TimeDelta::FromMilliseconds(10),
          base::TimeDelta::FromSeconds(1000),
          100);
    }
  }

  // Appends the urls in |urls| to |browser|.
  void AppendURLsToBrowser(Browser* browser,
                           const std::vector<GURL>& urls) {
    for (size_t i = 0; i < urls.size(); ++i) {
      int add_types = TabStripModel::ADD_FORCE_INDEX;
      if (i == 0)
        add_types |= TabStripModel::ADD_ACTIVE;
      chrome::NavigateParams params(browser, urls[i],
                                    content::PAGE_TRANSITION_AUTO_TOPLEVEL);
      params.disposition = i == 0 ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
      params.tabstrip_add_types = add_types;
      chrome::Navigate(&params);
    }
  }

  // Invokes TabRestored on the SessionService for all tabs in browser after
  // initial_count.
  void NotifySessionServiceOfRestoredTabs(Browser* browser, int initial_count) {
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile_);
    if (!session_service)
      return;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int i = initial_count; i < tab_strip->count(); ++i)
      session_service->TabRestored(tab_strip->GetWebContentsAt(i),
                                   tab_strip->IsTabPinned(i));
  }

  // The profile to create the sessions for.
  Profile* profile_;

  // The first browser to restore to, may be null.
  Browser* browser_;

  // The desktop on which all new browsers should be created (browser_, if it is
  // not NULL, must be of this desktop type as well).
  chrome::HostDesktopType host_desktop_type_;

  // Whether or not restore is synchronous.
  const bool synchronous_;

  // See description of CLOBBER_CURRENT_TAB.
  const bool clobber_existing_tab_;

  // If true and there is an error or there are no windows to restore, we
  // create a tabbed browser anyway. This is used on startup to make sure at
  // at least one window is created.
  const bool always_create_tabbed_browser_;

  // Set of URLs to open in addition to those restored from the session.
  std::vector<GURL> urls_to_open_;

  // Used to get the session.
  CancelableTaskTracker cancelable_task_tracker_;

  // Responsible for loading the tabs.
  scoped_refptr<TabLoader> tab_loader_;

  // When synchronous we run a nested message loop. To avoid creating windows
  // from the nested message loop (which can make exiting the nested message
  // loop take a while) we cache the SessionWindows here and create the actual
  // windows when the nested message loop exits.
  std::vector<SessionWindow*> windows_;
  SessionID::id_type active_window_id_;

  content::NotificationRegistrar registrar_;

  // The time we started the restore.
  base::TimeTicks restore_started_;

  // Set to true after the first browser is shown.
  bool browser_shown_;

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreImpl);
};

}  // namespace

// SessionRestore -------------------------------------------------------------

// static
Browser* SessionRestore::RestoreSession(
    Profile* profile,
    Browser* browser,
    chrome::HostDesktopType host_desktop_type,
    uint32 behavior,
    const std::vector<GURL>& urls_to_open) {
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "SessionRestore-Start", false);
#endif
  DCHECK(profile);
  // Always restore from the original profile (incognito profiles have no
  // session service).
  profile = profile->GetOriginalProfile();
  if (!SessionServiceFactory::GetForProfile(profile)) {
    NOTREACHED();
    return NULL;
  }
  profile->set_restored_last_session(true);
  // SessionRestoreImpl takes care of deleting itself when done.
  SessionRestoreImpl* restorer = new SessionRestoreImpl(
      profile, browser, host_desktop_type, (behavior & SYNCHRONOUS) != 0,
      (behavior & CLOBBER_CURRENT_TAB) != 0,
      (behavior & ALWAYS_CREATE_TABBED_BROWSER) != 0,
      urls_to_open);
  return restorer->Restore();
}

// static
void SessionRestore::RestoreForeignSessionWindows(
    Profile* profile,
    chrome::HostDesktopType host_desktop_type,
    std::vector<const SessionWindow*>::const_iterator begin,
    std::vector<const SessionWindow*>::const_iterator end) {
  std::vector<GURL> gurls;
  SessionRestoreImpl restorer(profile,
      static_cast<Browser*>(NULL), host_desktop_type, true, false, true, gurls);
  restorer.RestoreForeignSession(begin, end);
}

// static
void SessionRestore::RestoreForeignSessionTab(
    content::WebContents* source_web_contents,
    const SessionTab& tab,
    WindowOpenDisposition disposition) {
  Browser* browser = chrome::FindBrowserWithWebContents(source_web_contents);
  Profile* profile = browser->profile();
  std::vector<GURL> gurls;
  SessionRestoreImpl restorer(profile, browser, browser->host_desktop_type(),
                              true, false, false, gurls);
  restorer.RestoreForeignTab(tab, disposition);
}

// static
bool SessionRestore::IsRestoring(const Profile* profile) {
  if (active_session_restorers == NULL)
    return false;
  for (std::set<SessionRestoreImpl*>::const_iterator it =
           active_session_restorers->begin();
       it != active_session_restorers->end(); ++it) {
    if ((*it)->profile() == profile)
      return true;
  }
  return false;
}
