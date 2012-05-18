// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_shutdown.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

using content::WebContents;

namespace {

// This object is instantiated when the first Browser object is added to the
// list and delete when the last one is removed. It watches for loads and
// creates histograms of some global object counts.
class BrowserActivityObserver : public content::NotificationObserver {
 public:
  BrowserActivityObserver() {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }
  ~BrowserActivityObserver() {}

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
      const content::LoadCommittedDetails& load =
          *content::Details<content::LoadCommittedDetails>(details).ptr();
      if (!load.is_navigation_to_different_page())
        return;  // Don't log for subframes or other trivial types.

      LogRenderProcessHostCount();
      LogBrowserTabCount();
    } else if (type == content::NOTIFICATION_APP_TERMINATING) {
      delete this;
    }
  }

  // Counts the number of active RenderProcessHosts and logs them.
  void LogRenderProcessHostCount() const {
    int hosts_count = 0;
    for (content::RenderProcessHost::iterator i(
            content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance())
      ++hosts_count;
    UMA_HISTOGRAM_CUSTOM_COUNTS("MPArch.RPHCountPerLoad", hosts_count,
                                1, 50, 50);
  }

  // Counts the number of tabs in each browser window and logs them. This is
  // different than the number of WebContents objects since WebContents objects
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

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActivityObserver);
};

BrowserActivityObserver* activity_observer = NULL;

static BrowserList::BrowserVector& browsers() {
  CR_DEFINE_STATIC_LOCAL(BrowserList::BrowserVector, browser_vector, ());
  return browser_vector;
}

static BrowserList::BrowserVector& last_active_browsers() {
  CR_DEFINE_STATIC_LOCAL(BrowserList::BrowserVector, last_active_vector, ());
  return last_active_vector;
}

static ObserverList<BrowserList::Observer>& observers() {
  CR_DEFINE_STATIC_LOCAL(
      ObserverList<BrowserList::Observer>, observer_vector, ());
  return observer_vector;
}

printing::BackgroundPrintingManager* GetBackgroundPrintingManager() {
  return g_browser_process->background_printing_manager();
}

}  // namespace

// static
void BrowserList::AddBrowser(Browser* browser) {
  DCHECK(browser);
  browsers().push_back(browser);

  g_browser_process->AddRefModule();

  if (!activity_observer)
    activity_observer = new BrowserActivityObserver;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::Source<Browser>(browser),
      content::NotificationService::NoDetails());

  // Send out notifications after add has occurred. Do some basic checking to
  // try to catch evil observers that change the list from under us.
  size_t original_count = observers().size();
  FOR_EACH_OBSERVER(Observer, observers(), OnBrowserAdded(browser));
  DCHECK_EQ(original_count, observers().size())
      << "observer list modified during notification";
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers());

  // Many UI tests rely on closing the last browser window quitting the
  // application.
  // Mac: Closing all windows does not indicate quitting the application. Lie
  // for now and ignore behavior outside of unit tests.
  // ChromeOS: Force closing last window to close app with flag.
  // TODO(andybons | pkotwicz): Fix the UI tests to Do The Right Thing.
#if defined(OS_CHROMEOS)
  bool closing_app;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableZeroBrowsersOpenForTests))
    closing_app = (browsers().size() == 1);
  else
    closing_app = (browsers().size() == 1 &&
        browser_shutdown::IsTryingToQuit());
#else
  bool closing_app = (browsers().size() == 1);
#endif  // OS_CHROMEOS

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser),
      content::Details<bool>(&closing_app));

  RemoveBrowserFrom(browser, &browsers());

  // Do some basic checking to try to catch evil observers
  // that change the list from under us.
  size_t original_count = observers().size();
  FOR_EACH_OBSERVER(Observer, observers(), OnBrowserRemoved(browser));
  DCHECK_EQ(original_count, observers().size())
      << "observer list modified during notification";

  g_browser_process->ReleaseModule();

  // If we're exiting, send out the APP_TERMINATING notification to allow other
  // modules to shut themselves down.
  if (browsers().empty() &&
      (browser_shutdown::IsTryingToQuit() ||
       g_browser_process->IsShuttingDown())) {
    // Last browser has just closed, and this is a user-initiated quit or there
    // is no module keeping the app alive, so send out our notification. No need
    // to call ProfileManager::ShutdownSessionServices() as part of the
    // shutdown, because Browser::WindowClosing() already makes sure that the
    // SessionService is created and notified.
    browser::NotifyAppTerminating();
    browser::OnAppExiting();
  }
}

// static
void BrowserList::AddObserver(BrowserList::Observer* observer) {
  observers().AddObserver(observer);
}

// static
void BrowserList::RemoveObserver(BrowserList::Observer* observer) {
  observers().RemoveObserver(observer);
}

void BrowserList::CloseAllBrowsersWithProfile(Profile* profile) {
  BrowserVector browsers_to_close;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->profile()->GetOriginalProfile() == profile->GetOriginalProfile())
      browsers_to_close.push_back(*i);
  }

  for (BrowserVector::const_iterator i = browsers_to_close.begin();
       i != browsers_to_close.end(); ++i) {
    (*i)->window()->Close();
  }
}

// static
BrowserList::const_iterator BrowserList::begin() {
  return browsers().begin();
}

// static
BrowserList::const_iterator BrowserList::end() {
  return browsers().end();
}

// static
bool BrowserList::empty() {
  return browsers().empty();
}

// static
size_t BrowserList::size() {
  return browsers().size();
}

// static
void BrowserList::SetLastActive(Browser* browser) {
  // If the browser is currently trying to quit, we don't want to set the last
  // active browser because that can alter the last active browser that the user
  // intended depending on the order in which the windows close.
  if (browser_shutdown::IsTryingToQuit())
    return;
  RemoveBrowserFrom(browser, &last_active_browsers());
  last_active_browsers().push_back(browser);

  FOR_EACH_OBSERVER(Observer, observers(), OnBrowserSetLastActive(browser));
}

// static
Browser* BrowserList::GetLastActive() {
  if (!last_active_browsers().empty())
    return *(last_active_browsers().rbegin());

  return NULL;
}

// static
BrowserList::const_reverse_iterator BrowserList::begin_last_active() {
  return last_active_browsers().rbegin();
}

// static
BrowserList::const_reverse_iterator BrowserList::end_last_active() {
  return last_active_browsers().rend();
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
bool BrowserList::IsOffTheRecordSessionActiveForProfile(Profile* profile) {
#if defined(OS_CHROMEOS)
  // In ChromeOS, we assume that the default profile is always valid, so if
  // we are in guest mode, keep the OTR profile active so it won't be deleted.
  if (chromeos::UserManager::Get()->IsLoggedInAsGuest())
    return true;
#endif
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->profile()->IsSameProfile(profile) &&
        (*i)->profile()->IsOffTheRecord()) {
      return true;
    }
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
      bg_printing_iterator_(GetBackgroundPrintingManager()->begin()),
      cur_(NULL) {
  Advance();
}

void TabContentsIterator::Advance() {
  // The current WebContents should be valid unless we are at the beginning.
  DCHECK(cur_ || (web_view_index_ == -1 &&
                  browser_iterator_ == BrowserList::begin()))
      << "Trying to advance past the end";

  // Update cur_ to the next WebContents in the list.
  while (browser_iterator_ != BrowserList::end()) {
    if (++web_view_index_ >= (*browser_iterator_)->tab_count()) {
      // Advance to the next Browser in the list.
      ++browser_iterator_;
      web_view_index_ = -1;
      continue;
    }

    TabContentsWrapper* next_tab =
        (*browser_iterator_)->GetTabContentsWrapperAt(web_view_index_);
    if (next_tab) {
      cur_ = next_tab;
      return;
    }
  }
  // If no more WebContents from Browsers, check the BackgroundPrintingManager.
  while (bg_printing_iterator_ != GetBackgroundPrintingManager()->end()) {
    cur_ = *bg_printing_iterator_;
    CHECK(cur_);
    ++bg_printing_iterator_;
    return;
  }
  // Reached the end - no more WebContents.
  cur_ = NULL;
}
