// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"

using base::UserMetricsAction;
using content::WebContents;

// static
base::LazyInstance<base::ObserverList<chrome::BrowserListObserver>>::Leaky
    BrowserList::observers_ = LAZY_INSTANCE_INITIALIZER;

// static
BrowserList* BrowserList::instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// BrowserList, public:

Browser* BrowserList::GetLastActive() const {
  if (!last_active_browsers_.empty())
    return *(last_active_browsers_.rbegin());
  return NULL;
}

// static
BrowserList* BrowserList::GetInstance() {
  BrowserList** list = NULL;
  list = &instance_;
  if (!*list)
    *list = new BrowserList;
  return *list;
}

// static
void BrowserList::AddBrowser(Browser* browser) {
  DCHECK(browser);
  GetInstance()->browsers_.push_back(browser);

  g_browser_process->AddRefModule();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::Source<Browser>(browser),
      content::NotificationService::NoDetails());

  FOR_EACH_OBSERVER(chrome::BrowserListObserver, observers_.Get(),
                    OnBrowserAdded(browser));
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  // Remove |browser| from the appropriate list instance.
  BrowserList* browser_list = GetInstance();
  RemoveBrowserFrom(browser, &browser_list->last_active_browsers_);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser),
      content::NotificationService::NoDetails());

  RemoveBrowserFrom(browser, &browser_list->browsers_);

  FOR_EACH_OBSERVER(chrome::BrowserListObserver, observers_.Get(),
                    OnBrowserRemoved(browser));

  g_browser_process->ReleaseModule();

  // If we're exiting, send out the APP_TERMINATING notification to allow other
  // modules to shut themselves down.
  if (chrome::GetTotalBrowserCount() == 0 &&
      (browser_shutdown::IsTryingToQuit() ||
       g_browser_process->IsShuttingDown())) {
    // Last browser has just closed, and this is a user-initiated quit or there
    // is no module keeping the app alive, so send out our notification. No need
    // to call ProfileManager::ShutdownSessionServices() as part of the
    // shutdown, because Browser::WindowClosing() already makes sure that the
    // SessionService is created and notified.
    chrome::NotifyAppTerminating();
    chrome::OnAppExiting();
  }
}

// static
void BrowserList::AddObserver(chrome::BrowserListObserver* observer) {
  observers_.Get().AddObserver(observer);
}

// static
void BrowserList::RemoveObserver(chrome::BrowserListObserver* observer) {
  observers_.Get().RemoveObserver(observer);
}

// static
void BrowserList::CloseAllBrowsersWithProfile(Profile* profile) {
  BrowserVector browsers_to_close;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() ==
        profile->GetOriginalProfile())
      browsers_to_close.push_back(browser);
  }

  for (BrowserVector::const_iterator it = browsers_to_close.begin();
       it != browsers_to_close.end(); ++it) {
    (*it)->window()->Close();
  }
}

// static
void BrowserList::CloseAllBrowsersWithProfile(Profile* profile,
    const base::Callback<void(const base::FilePath&)>& on_close_success) {
  BrowserVector browsers_to_close;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() ==
        profile->GetOriginalProfile())
      browsers_to_close.push_back(browser);
  }

  TryToCloseBrowserList(browsers_to_close,
                        on_close_success,
                        profile->GetPath());
}

// static
void BrowserList::TryToCloseBrowserList(const BrowserVector& browsers_to_close,
    const base::Callback<void(const base::FilePath&)>& on_close_success,
    const base::FilePath& profile_path) {
  for (BrowserVector::const_iterator it = browsers_to_close.begin();
       it != browsers_to_close.end(); ++it) {
    if ((*it)->CallBeforeUnloadHandlers(
            base::Bind(&BrowserList::PostBeforeUnloadHandlers,
                       browsers_to_close,
                       on_close_success,
                       profile_path))) {
      return;
    }
  }

  on_close_success.Run(profile_path);

  for (Browser* b : browsers_to_close) {
    // BeforeUnload handlers may close browser windows, so we need to explicitly
    // check whether they still exist.
    if (b->window())
      b->window()->Close();
  }
}

// static
void BrowserList::PostBeforeUnloadHandlers(
    const BrowserVector& browsers_to_close,
    const base::Callback<void(const base::FilePath&)>& on_close_success,
    const base::FilePath& profile_path,
    bool tab_close_confirmed) {
  // We need this bool to avoid infinite recursion when resetting the
  // BeforeUnload handlers, since doing that will trigger calls back to this
  // method for each affected window.
  static bool resetting_handlers = false;

  if (tab_close_confirmed) {
    TryToCloseBrowserList(browsers_to_close, on_close_success, profile_path);
  } else if (!resetting_handlers) {
    base::AutoReset<bool> resetting_handlers_scoper(&resetting_handlers, true);
    for (BrowserVector::const_iterator it = browsers_to_close.begin();
         it != browsers_to_close.end(); ++it) {
      (*it)->ResetBeforeUnloadHandlers();
    }
  }
}

// static
void BrowserList::SetLastActive(Browser* browser) {
  content::RecordAction(UserMetricsAction("ActiveBrowserChanged"));

  RemoveBrowserFrom(browser, &GetInstance()->last_active_browsers_);
  GetInstance()->last_active_browsers_.push_back(browser);

  FOR_EACH_OBSERVER(chrome::BrowserListObserver, observers_.Get(),
                    OnBrowserSetLastActive(browser));
}

// static
bool BrowserList::IsOffTheRecordSessionActive() {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsOffTheRecord())
      return true;
  }
  return false;
}

// static
bool BrowserList::IsOffTheRecordSessionActiveForProfile(Profile* profile) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsSameProfile(profile) &&
        browser->profile()->IsOffTheRecord()) {
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserList, private:

BrowserList::BrowserList() {
}

BrowserList::~BrowserList() {
}

// static
void BrowserList::RemoveBrowserFrom(Browser* browser,
                                    BrowserVector* browser_list) {
  BrowserVector::iterator remove_browser =
      std::find(browser_list->begin(), browser_list->end(), browser);
  if (remove_browser != browser_list->end())
    browser_list->erase(remove_browser);
}
