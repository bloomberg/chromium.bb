// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"

// #include "build/build_config.h"
// #include "base/prefs/pref_service.h"

namespace chrome {

// static
BrowserListImpl* BrowserListImpl::native_instance_ = NULL;
BrowserListImpl* BrowserListImpl::ash_instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// BrowserListImpl, public:

// static
BrowserListImpl* BrowserListImpl::GetInstance(HostDesktopType type) {
  BrowserListImpl** list = NULL;
  if (type == HOST_DESKTOP_TYPE_NATIVE)
    list = &native_instance_;
  else if (type == HOST_DESKTOP_TYPE_ASH)
    list = &ash_instance_;
  else
    NOTREACHED();
  if (!*list)
    *list = new BrowserListImpl;
  return *list;
}

void BrowserListImpl::AddBrowser(Browser* browser) {
  DCHECK(browser);
  browsers_.push_back(browser);

  g_browser_process->AddRefModule();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::Source<Browser>(browser),
      content::NotificationService::NoDetails());

  // Send out notifications after add has occurred. Do some basic checking to
  // try to catch evil observers that change the list from under us.
  size_t original_count = observers_.size();
  FOR_EACH_OBSERVER(BrowserListObserver, observers_, OnBrowserAdded(browser));
  DCHECK_EQ(original_count, observers_.size())
      << "observer list modified during notification";
}

void BrowserListImpl::RemoveBrowser(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers_);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser),
      content::NotificationService::NoDetails());

  RemoveBrowserFrom(browser, &browsers_);

  FOR_EACH_OBSERVER(BrowserListObserver, observers_, OnBrowserRemoved(browser));

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
    chrome::NotifyAppTerminating();
    chrome::OnAppExiting();
  }
}

void BrowserListImpl::AddObserver(BrowserListObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserListImpl::RemoveObserver(BrowserListObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BrowserListImpl::SetLastActive(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers_);
  last_active_browsers_.push_back(browser);

  FOR_EACH_OBSERVER(BrowserListObserver, observers_,
                    OnBrowserSetLastActive(browser));
}

Browser* BrowserListImpl::GetLastActive() const {
  if (!last_active_browsers_.empty())
    return *(last_active_browsers_.rbegin());
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserListImpl, private:

BrowserListImpl::BrowserListImpl() {
}

BrowserListImpl::~BrowserListImpl() {
}

void BrowserListImpl::RemoveBrowserFrom(Browser* browser,
                                        BrowserVector* browser_list) {
  const iterator remove_browser =
      std::find(browser_list->begin(), browser_list->end(), browser);
  if (remove_browser != browser_list->end())
    browser_list->erase(remove_browser);
}

}  // namespace chrome
