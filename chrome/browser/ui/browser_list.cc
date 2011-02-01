// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_list.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/result_codes.h"

#if defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/update_library.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#endif

namespace {

// This object is instantiated when the first Browser object is added to the
// list and delete when the last one is removed. It watches for loads and
// creates histograms of some global object counts.
class BrowserActivityObserver : public NotificationObserver {
 public:
  BrowserActivityObserver() {
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   NotificationService::AllSources());
  }
  ~BrowserActivityObserver() {}

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::NAV_ENTRY_COMMITTED);
    const NavigationController::LoadCommittedDetails& load =
        *Details<NavigationController::LoadCommittedDetails>(details).ptr();
    if (!load.is_main_frame || load.is_auto || load.is_in_page)
      return;  // Don't log for subframes or other trivial types.

    LogRenderProcessHostCount();
    LogBrowserTabCount();
  }

  // Counts the number of active RenderProcessHosts and logs them.
  void LogRenderProcessHostCount() const {
    int hosts_count = 0;
    for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance())
      ++hosts_count;
    UMA_HISTOGRAM_CUSTOM_COUNTS("MPArch.RPHCountPerLoad", hosts_count,
                                1, 50, 50);
  }

  // Counts the number of tabs in each browser window and logs them. This is
  // different than the number of TabContents objects since TabContents objects
  // can be used for popups and in dialog boxes. We're just counting toplevel
  // tabs here.
  void LogBrowserTabCount() const {
    int tab_count = 0;
    for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
         browser_iterator != BrowserList::end(); browser_iterator++) {
      // Record how many tabs each window has open.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerWindow",
                                  (*browser_iterator)->tab_count(), 1, 200, 50);
      tab_count += (*browser_iterator)->tab_count();
    }
    // Record how many tabs total are open (across all windows).
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tab_count, 1, 200, 50);

    Browser* browser = BrowserList::GetLastActive();
    if (browser) {
      // Record how many tabs the active window has open.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountActiveWindow",
                                  browser->tab_count(), 1, 200, 50);
    }
  }

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActivityObserver);
};

BrowserActivityObserver* activity_observer = NULL;

// Type used to indicate only the type should be matched.
const int kMatchNothing                 = 0;

// See BrowserMatches for details.
const int kMatchOriginalProfile         = 1 << 0;

// See BrowserMatches for details.
const int kMatchCanSupportWindowFeature = 1 << 1;

// Returns true if the specified |browser| matches the specified arguments.
// |match_types| is a bitmask dictating what parameters to match:
// . If it contains kMatchOriginalProfile then the original profile of the
//   browser must match |profile->GetOriginalProfile()|. This is used to match
//   incognito windows.
// . If it contains kMatchCanSupportWindowFeature
//   |CanSupportWindowFeature(window_feature)| must return true.
bool BrowserMatches(Browser* browser,
                    Profile* profile,
                    Browser::Type type,
                    Browser::WindowFeature window_feature,
                    uint32 match_types) {
  if (match_types & kMatchCanSupportWindowFeature &&
      !browser->CanSupportWindowFeature(window_feature)) {
    return false;
  }

  if (match_types & kMatchOriginalProfile) {
    if (browser->profile()->GetOriginalProfile() !=
        profile->GetOriginalProfile())
      return false;
  } else if (browser->profile() != profile) {
    return false;
  }

  if (type != Browser::TYPE_ANY && browser->type() != type)
    return false;

  return true;
}

// Returns the first browser in the specified iterator that returns true from
// |BrowserMatches|, or null if no browsers match the arguments. See
// |BrowserMatches| for details on the arguments.
template <class T>
Browser* FindBrowserMatching(const T& begin,
                             const T& end,
                             Profile* profile,
                             Browser::Type type,
                             Browser::WindowFeature window_feature,
                             uint32 match_types) {
  for (T i = begin; i != end; ++i) {
    if (BrowserMatches(*i, profile, type, window_feature, match_types))
      return *i;
  }
  return NULL;
}

}  // namespace

BrowserList::BrowserVector BrowserList::browsers_;
ObserverList<BrowserList::Observer> BrowserList::observers_;

// static
void BrowserList::AddBrowser(Browser* browser) {
  DCHECK(browser);
  browsers_.push_back(browser);

  g_browser_process->AddRefModule();

  if (!activity_observer)
    activity_observer = new BrowserActivityObserver;

  NotificationService::current()->Notify(
      NotificationType::BROWSER_OPENED,
      Source<Browser>(browser),
      NotificationService::NoDetails());

  // Send out notifications after add has occurred. Do some basic checking to
  // try to catch evil observers that change the list from under us.
  size_t original_count = observers_.size();
  FOR_EACH_OBSERVER(Observer, observers_, OnBrowserAdded(browser));
  DCHECK_EQ(original_count, observers_.size())
      << "observer list modified during notification";
}

// static
void BrowserList::MarkAsCleanShutdown() {
  for (const_iterator i = begin(); i != end(); ++i) {
    (*i)->profile()->MarkAsCleanShutdown();
  }
}

#if defined(OS_CHROMEOS)
// static
void BrowserList::NotifyWindowManagerAboutSignout() {
  static bool notified = false;
  if (!notified) {
    // Let the window manager know that we're going away before we start closing
    // windows so it can display a graceful transition to a black screen.
    chromeos::WmIpc::instance()->NotifyAboutSignout();
    notified = true;
  }
}
#endif

// static
void BrowserList::NotifyAndTerminate(bool fast_path) {
#if defined(OS_CHROMEOS)
  NotifyWindowManagerAboutSignout();
#endif

  if (fast_path) {
    NotificationService::current()->Notify(NotificationType::APP_TERMINATING,
                                           NotificationService::AllSources(),
                                           NotificationService::NoDetails());
  }

#if defined(OS_CHROMEOS)
  chromeos::CrosLibrary* cros_library = chromeos::CrosLibrary::Get();
  if (cros_library->EnsureLoaded()) {
    // If update has been installed, reboot, otherwise, sign out.
    if (cros_library->GetUpdateLibrary()->status().status ==
          chromeos::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
      cros_library->GetUpdateLibrary()->RebootAfterUpdate();
    } else {
      cros_library->GetLoginLibrary()->StopSession("");
    }
    return;
  }
  // If running the Chrome OS build, but we're not on the device, fall through
#endif
  AllBrowsersClosedAndAppExiting();
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers_);

  // Closing all windows does not indicate quitting the application on the Mac,
  // however, many UI tests rely on this behavior so leave it be for now and
  // simply ignore the behavior on the Mac outside of unit tests.
  // TODO(andybons): Fix the UI tests to Do The Right Thing.
  bool closing_last_browser = (browsers_.size() == 1);
  NotificationService::current()->Notify(
      NotificationType::BROWSER_CLOSED,
      Source<Browser>(browser), Details<bool>(&closing_last_browser));

  RemoveBrowserFrom(browser, &browsers_);

  // Do some basic checking to try to catch evil observers
  // that change the list from under us.
  size_t original_count = observers_.size();
  FOR_EACH_OBSERVER(Observer, observers_, OnBrowserRemoved(browser));
  DCHECK_EQ(original_count, observers_.size())
      << "observer list modified during notification";

  // If the last Browser object was destroyed, make sure we try to close any
  // remaining dependent windows too.
  if (browsers_.empty()) {
    delete activity_observer;
    activity_observer = NULL;
  }

  g_browser_process->ReleaseModule();

  // If we're exiting, send out the APP_TERMINATING notification to allow other
  // modules to shut themselves down.
  if (browsers_.empty() &&
      (browser_shutdown::IsTryingToQuit() ||
       g_browser_process->IsShuttingDown())) {
    // Last browser has just closed, and this is a user-initiated quit or there
    // is no module keeping the app alive, so send out our notification. No need
    // to call ProfileManager::ShutdownSessionServices() as part of the
    // shutdown, because Browser::WindowClosing() already makes sure that the
    // SessionService is created and notified.
    NotificationService::current()->Notify(NotificationType::APP_TERMINATING,
                                           NotificationService::AllSources(),
                                           NotificationService::NoDetails());
    AllBrowsersClosedAndAppExiting();
  }
}

// static
void BrowserList::AddObserver(BrowserList::Observer* observer) {
  observers_.AddObserver(observer);
}

// static
void BrowserList::RemoveObserver(BrowserList::Observer* observer) {
  observers_.RemoveObserver(observer);
}

#if defined(OS_CHROMEOS)
// static
bool BrowserList::NeedBeforeUnloadFired() {
  bool need_before_unload_fired = false;
  for (const_iterator i = begin(); i != end(); ++i) {
    need_before_unload_fired = need_before_unload_fired ||
      (*i)->TabsNeedBeforeUnloadFired();
  }
  return need_before_unload_fired;
}

// static
bool BrowserList::PendingDownloads() {
  for (const_iterator i = begin(); i != end(); ++i) {
    bool normal_downloads_are_present = false;
    bool incognito_downloads_are_present = false;
    (*i)->CheckDownloadsInProgress(&normal_downloads_are_present,
                                   &incognito_downloads_are_present);
    if (normal_downloads_are_present || incognito_downloads_are_present)
      return true;
  }
  return false;
}
#endif

// static
void BrowserList::CloseAllBrowsers() {
  bool session_ending =
      browser_shutdown::GetShutdownType() == browser_shutdown::END_SESSION;
  bool use_post = !session_ending;
  bool force_exit = false;
#if defined(USE_X11)
  if (session_ending)
    force_exit = true;
#endif
  // Tell everyone that we are shutting down.
  browser_shutdown::SetTryingToQuit(true);

  // Before we close the browsers shutdown all session services. That way an
  // exit can restore all browsers open before exiting.
  ProfileManager::ShutdownSessionServices();

  // If there are no browsers, send the APP_TERMINATING action here. Otherwise,
  // it will be sent by RemoveBrowser() when the last browser has closed.
  if (force_exit || browsers_.empty()) {
    NotifyAndTerminate(true);
    return;
  }
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker(
      "StartedClosingWindows", false);
#endif
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end();) {
    Browser* browser = *i;
    browser->window()->Close();
    if (use_post) {
      ++i;
    } else {
      // This path is hit during logoff/power-down. In this case we won't get
      // a final message and so we force the browser to be deleted.
      // Close doesn't immediately destroy the browser
      // (Browser::TabStripEmpty() uses invoke later) but when we're ending the
      // session we need to make sure the browser is destroyed now. So, invoke
      // DestroyBrowser to make sure the browser is deleted and cleanup can
      // happen.
      browser->window()->DestroyBrowser();
      i = BrowserList::begin();
      if (i != BrowserList::end() && browser == *i) {
        // Destroying the browser should have removed it from the browser list.
        // We should never get here.
        NOTREACHED();
        return;
      }
    }
  }
}

// static
void BrowserList::Exit() {
#if defined(OS_CHROMEOS)
  // Fast shutdown for ChromeOS when there's no unload processing to be done.
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()
      && !NeedBeforeUnloadFired()
      && !PendingDownloads()) {
    NotifyAndTerminate(true);
    return;
  }
#endif
  CloseAllBrowsersAndExit();
}

// static
void BrowserList::CloseAllBrowsersAndExit() {
  MarkAsCleanShutdown();  // Don't notify users of crashes beyond this point.
  NotificationService::current()->Notify(
      NotificationType::APP_EXITING,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

#if !defined(OS_MACOSX)
  // On most platforms, closing all windows causes the application to exit.
  CloseAllBrowsers();
#else
  // On the Mac, the application continues to run once all windows are closed.
  // Terminate will result in a CloseAllBrowsers() call, and once (and if)
  // that is done, will cause the application to exit cleanly.
  chrome_browser_application_mac::Terminate();
#endif
}

// static
void BrowserList::SessionEnding() {
  // EndSession is invoked once per frame. Only do something the first time.
  static bool already_ended = false;
  // We may get called in the middle of shutdown, e.g. http://crbug.com/70852
  // In this case, do nothing.
  if (already_ended || !NotificationService::current())
    return;
  already_ended = true;

  browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);

  NotificationService::current()->Notify(
      NotificationType::APP_EXITING,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

  // Write important data first.
  g_browser_process->EndSession();

  BrowserList::CloseAllBrowsers();

  // Send out notification. This is used during testing so that the test harness
  // can properly shutdown before we exit.
  NotificationService::current()->Notify(
      NotificationType::SESSION_END,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

  // And shutdown.
  browser_shutdown::Shutdown();

#if defined(OS_WIN)
  // At this point the message loop is still running yet we've shut everything
  // down. If any messages are processed we'll likely crash. Exit now.
  ExitProcess(ResultCodes::NORMAL_EXIT);
#elif defined(OS_LINUX)
  _exit(ResultCodes::NORMAL_EXIT);
#else
  NOTIMPLEMENTED();
#endif
}

// static
bool BrowserList::HasBrowserWithProfile(Profile* profile) {
  return FindBrowserMatching(BrowserList::begin(),
                             BrowserList::end(),
                             profile, Browser::TYPE_ANY,
                             Browser::FEATURE_NONE,
                             kMatchNothing) != NULL;
}

// static
int BrowserList::keep_alive_count_ = 0;

// static
void BrowserList::StartKeepAlive() {
  // Increment the browser process refcount as long as we're keeping the
  // application alive.
  if (!WillKeepAlive())
    g_browser_process->AddRefModule();
  keep_alive_count_++;
}

// static
void BrowserList::EndKeepAlive() {
  DCHECK_GT(keep_alive_count_, 0);
  keep_alive_count_--;
  // Allow the app to shutdown again.
  if (!WillKeepAlive()) {
    g_browser_process->ReleaseModule();
    // If there are no browsers open and we aren't already shutting down,
    // initiate a shutdown. Also skips shutdown if this is a unit test
    // (MessageLoop::current() == null).
    if (browsers_.empty() && !browser_shutdown::IsTryingToQuit() &&
        MessageLoop::current())
      CloseAllBrowsers();
  }
}

// static
bool BrowserList::WillKeepAlive() {
  return keep_alive_count_ > 0;
}

// static
BrowserList::BrowserVector BrowserList::last_active_browsers_;

// static
void BrowserList::SetLastActive(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers_);
  last_active_browsers_.push_back(browser);

  FOR_EACH_OBSERVER(Observer, observers_, OnBrowserSetLastActive(browser));
}

// static
Browser* BrowserList::GetLastActive() {
  if (!last_active_browsers_.empty())
    return *(last_active_browsers_.rbegin());

  return NULL;
}

// static
Browser* BrowserList::GetLastActiveWithProfile(Profile* p) {
  // We are only interested in last active browsers, so we don't fall back to
  // all browsers like FindBrowserWith* do.
  return FindBrowserMatching(
      BrowserList::begin_last_active(), BrowserList::end_last_active(), p,
      Browser::TYPE_ANY, Browser::FEATURE_NONE, kMatchNothing);
}

// static
Browser* BrowserList::FindBrowserWithType(Profile* p, Browser::Type t,
                                          bool match_incognito) {
  uint32 match_types = match_incognito ? kMatchOriginalProfile : kMatchNothing;
  Browser* browser = FindBrowserMatching(
      BrowserList::begin_last_active(), BrowserList::end_last_active(),
      p, t, Browser::FEATURE_NONE, match_types);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser :
      FindBrowserMatching(BrowserList::begin(), BrowserList::end(), p, t,
                          Browser::FEATURE_NONE, match_types);
}

// static
Browser* BrowserList::FindBrowserWithFeature(Profile* p,
                                             Browser::WindowFeature feature) {
  Browser* browser = FindBrowserMatching(
      BrowserList::begin_last_active(), BrowserList::end_last_active(),
      p, Browser::TYPE_ANY, feature, kMatchCanSupportWindowFeature);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser :
      FindBrowserMatching(BrowserList::begin(), BrowserList::end(), p,
                          Browser::TYPE_ANY, feature,
                          kMatchCanSupportWindowFeature);
}

// static
Browser* BrowserList::FindBrowserWithProfile(Profile* p) {
  return FindBrowserWithType(p, Browser::TYPE_ANY, false);
}

// static
Browser* BrowserList::FindBrowserWithID(SessionID::id_type desired_id) {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->session_id().id() == desired_id)
      return *i;
  }
  return NULL;
}

// static
size_t BrowserList::GetBrowserCountForType(Profile* p, Browser::Type type) {
  size_t result = 0;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (BrowserMatches(*i, p, type, Browser::FEATURE_NONE, kMatchNothing))
      ++result;
  }
  return result;
}

// static
size_t BrowserList::GetBrowserCount(Profile* p) {
  size_t result = 0;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (BrowserMatches(*i, p, Browser::TYPE_ANY, Browser::FEATURE_NONE,
                       kMatchNothing)) {
      result++;
    }
  }
  return result;
}

// static
bool BrowserList::IsOffTheRecordSessionActive() {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->profile()->IsOffTheRecord())
      return true;
  }
  return false;
}

// static
void BrowserList::RemoveBrowserFrom(Browser* browser,
                                    BrowserVector* browser_list) {
  const iterator remove_browser =
      std::find(browser_list->begin(), browser_list->end(), browser);
  if (remove_browser != browser_list->end())
    browser_list->erase(remove_browser);
}

TabContentsIterator::TabContentsIterator()
    : browser_iterator_(BrowserList::begin()),
      web_view_index_(-1),
      cur_(NULL) {
    Advance();
  }

void TabContentsIterator::Advance() {
  // Unless we're at the beginning (index = -1) or end (iterator = end()),
  // then the current TabContents should be valid.
  DCHECK(web_view_index_ || browser_iterator_ == BrowserList::end() || cur_)
      << "Trying to advance past the end";

  // Update cur_ to the next TabContents in the list.
  while (browser_iterator_ != BrowserList::end()) {
    web_view_index_++;

    while (web_view_index_ >= (*browser_iterator_)->tab_count()) {
      // advance browsers
      ++browser_iterator_;
      web_view_index_ = 0;
      if (browser_iterator_ == BrowserList::end()) {
        cur_ = NULL;
        return;
      }
    }

    TabContents* next_tab =
        (*browser_iterator_)->GetTabContentsAt(web_view_index_);
    if (next_tab) {
      cur_ = next_tab;
      return;
    }
  }
}
