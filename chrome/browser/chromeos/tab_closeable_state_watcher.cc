// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tab_closeable_state_watcher.h"

#include "base/command_line.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/notification_service.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// TabCloseableStateWatcher::TabStripWatcher, public:

TabCloseableStateWatcher::TabStripWatcher::TabStripWatcher(
    TabCloseableStateWatcher* main_watcher, const Browser* browser)
    : main_watcher_(main_watcher),
      browser_(browser) {
  browser_->tabstrip_model()->AddObserver(this);
}

TabCloseableStateWatcher::TabStripWatcher::~TabStripWatcher() {
  browser_->tabstrip_model()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// TabCloseableStateWatcher::TabStripWatcher,
//     TabStripModelObserver implementation:

void TabCloseableStateWatcher::TabStripWatcher::TabInsertedAt(
    TabContentsWrapper* tab_contents, int index, bool foreground) {
  main_watcher_->OnTabStripChanged(browser_, false);
}

void TabCloseableStateWatcher::TabStripWatcher::TabClosingAt(
    TabStripModel* tab_strip_model,
    TabContentsWrapper* tab_contents,
    int index) {
  // Check if the last tab is closing.
  if (tab_strip_model->count() == 1)
    main_watcher_->OnTabStripChanged(browser_, true);
}

void TabCloseableStateWatcher::TabStripWatcher::TabDetachedAt(
    TabContentsWrapper* tab_contents, int index) {
  main_watcher_->OnTabStripChanged(browser_, false);
}

void TabCloseableStateWatcher::TabStripWatcher::TabChangedAt(
    TabContentsWrapper* tab_contents, int index, TabChangeType change_type) {
  main_watcher_->OnTabStripChanged(browser_, false);
}

////////////////////////////////////////////////////////////////////////////////
// TabCloseableStateWatcher, public:

TabCloseableStateWatcher::TabCloseableStateWatcher()
    : can_close_tab_(true),
      signing_off_(false),
      guest_session_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kGuestSession)),
      waiting_for_browser_(false) {
  BrowserList::AddObserver(this);
  notification_registrar_.Add(this, NotificationType::APP_EXITING,
      NotificationService::AllSources());
}

TabCloseableStateWatcher::~TabCloseableStateWatcher() {
  BrowserList::RemoveObserver(this);
  if (!browser_shutdown::ShuttingDownWithoutClosingBrowsers())
    DCHECK(tabstrip_watchers_.empty());
}

bool TabCloseableStateWatcher::CanCloseTab(const Browser* browser) const {
  return browser->type() != Browser::TYPE_NORMAL ? true :
      (can_close_tab_ || waiting_for_browser_);
}

bool TabCloseableStateWatcher::CanCloseBrowser(Browser* browser) {
  BrowserActionType action_type;
  bool can_close = CanCloseBrowserImpl(browser, &action_type);
  if (action_type == OPEN_WINDOW) {
    browser->NewWindow();
  } else if (action_type == OPEN_NTP) {
    // NTP will be opened before closing last tab (via TabStripModelObserver::
    // TabClosingAt), close all tabs now.
    browser->CloseAllTabs();
  }
  return can_close;
}

void TabCloseableStateWatcher::OnWindowCloseCanceled(Browser* browser) {
  // This could be a call to cancel APP_EXITING if user doesn't proceed with
  // unloading handler.
  if (signing_off_) {
    signing_off_ = false;
    CheckAndUpdateState(browser);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabCloseableStateWatcher, BrowserList::Observer implementation:

void TabCloseableStateWatcher::OnBrowserAdded(const Browser* browser) {
  waiting_for_browser_ = false;

  // Only normal browsers may affect closeable state.
  if (browser->type() != Browser::TYPE_NORMAL)
    return;

  // Create TabStripWatcher to observe tabstrip of new browser.
  tabstrip_watchers_.push_back(new TabStripWatcher(this, browser));

  // When a normal browser is just added, there's no tabs yet, so we wait till
  // TabInsertedAt notification to check for change in state.
}

void TabCloseableStateWatcher::OnBrowserRemoved(const Browser* browser) {
  // Only normal browsers may affect closeable state.
  if (browser->type() != Browser::TYPE_NORMAL)
    return;

  // Remove TabStripWatcher for browser that is being removed.
  for (std::vector<TabStripWatcher*>::iterator it = tabstrip_watchers_.begin();
       it != tabstrip_watchers_.end(); ++it) {
    if ((*it)->browser() == browser) {
      delete (*it);
      tabstrip_watchers_.erase(it);
      break;
    }
  }

  CheckAndUpdateState(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// TabCloseableStateWatcher, NotificationObserver implementation:

void TabCloseableStateWatcher::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  if (type.value != NotificationType::APP_EXITING)
    NOTREACHED();
  if (!signing_off_) {
    signing_off_ = true;
    SetCloseableState(true);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabCloseableStateWatcher, private

void TabCloseableStateWatcher::OnTabStripChanged(const Browser* browser,
    bool closing_last_tab) {
  if (waiting_for_browser_)
    return;

  if (!closing_last_tab) {
    CheckAndUpdateState(browser);
    return;
  }

  // Before closing last tab, open new window or NTP if necessary.
  BrowserActionType action_type;
  CanCloseBrowserImpl(browser, &action_type);
  if (action_type != NONE) {
    Browser* mutable_browser = const_cast<Browser*>(browser);
    if (action_type == OPEN_WINDOW)
      mutable_browser->NewWindow();
    else if (action_type == OPEN_NTP)
      mutable_browser->NewTab();
  }
}

void TabCloseableStateWatcher::CheckAndUpdateState(
    const Browser* browser_to_check) {
  if (waiting_for_browser_ || signing_off_ || tabstrip_watchers_.empty() ||
      (browser_to_check && browser_to_check->type() != Browser::TYPE_NORMAL))
    return;

  bool new_can_close;

  if (tabstrip_watchers_.size() > 1) {
    new_can_close = true;
  } else {  // There's only 1 normal browser.
    if (!browser_to_check)
      browser_to_check = tabstrip_watchers_[0]->browser();
    if (browser_to_check->profile()->IsOffTheRecord() && !guest_session_) {
      new_can_close = true;
    } else {
      TabStripModel* tabstrip_model = browser_to_check->tabstrip_model();
      if (tabstrip_model->count() == 1) {
        new_can_close =
            tabstrip_model->GetTabContentsAt(0)->tab_contents()->GetURL() !=
                GURL(chrome::kChromeUINewTabURL);  // Tab is not NewTabPage.
      } else {
        new_can_close = true;
      }
    }
  }

  SetCloseableState(new_can_close);
}

void TabCloseableStateWatcher::SetCloseableState(bool closeable) {
  if (can_close_tab_ == closeable)  // No change in state.
    return;

  can_close_tab_ = closeable;

  // Notify of change in tab closeable state.
  NotificationService::current()->Notify(
      NotificationType::TAB_CLOSEABLE_STATE_CHANGED,
      NotificationService::AllSources(),
      Details<bool>(&can_close_tab_));
}

bool TabCloseableStateWatcher::CanCloseBrowserImpl(
    const Browser* browser,
    BrowserActionType* action_type) {
  *action_type = NONE;

  // If we're waiting for a new browser allow the close.
  if (waiting_for_browser_)
    return true;

  // Browser is always closeable when signing off.
  if (signing_off_)
    return true;

  // Non-normal browsers are always closeable.
  if (browser->type() != Browser::TYPE_NORMAL)
    return true;

  // If this is not the last normal browser, it's always closeable.
  if (tabstrip_watchers_.size() > 1)
    return true;

  // If last normal browser is incognito, open a non-incognito window, and allow
  // closing of incognito one (if not guest). When this happens we need to wait
  // for the new browser before doing any other actions as the new browser may
  // be created by way of session restore, which is async.
  if (browser->profile()->IsOffTheRecord() && !guest_session_) {
    *action_type = OPEN_WINDOW;
    waiting_for_browser_ = true;
    return true;
  }

  // If tab is not closeable, browser is not closeable.
  if (!can_close_tab_)
    return false;

  // Otherwise, close existing tabs, and deny closing of browser.
  // TabClosingAt will open NTP when the last tab is being closed.
  *action_type = OPEN_NTP;
  return false;
}

}  // namespace chromeos
