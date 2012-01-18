// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tab_closeable_state_watcher.h"

#include "base/command_line.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

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
  notification_registrar_.Add(this, content::NOTIFICATION_APP_EXITING,
      content::NotificationService::AllSources());
}

TabCloseableStateWatcher::~TabCloseableStateWatcher() {
  BrowserList::RemoveObserver(this);
  if (!browser_shutdown::ShuttingDownWithoutClosingBrowsers())
    DCHECK(tabstrip_watchers_.empty());
}

bool TabCloseableStateWatcher::CanCloseTab(const Browser* browser) const {
  return browser->is_type_tabbed() ?
      (can_close_tab_ || waiting_for_browser_) : true;
}

bool TabCloseableStateWatcher::CanCloseTabs(const Browser* browser,
    std::vector<int>* indices) const {
  if (signing_off_ || waiting_for_browser_ || tabstrip_watchers_.size() > 1 ||
      !browser->is_type_tabbed() ||
      (browser->profile()->IsOffTheRecord() && !guest_session_))
    return true;

  if (!can_close_tab_) {
    indices->clear();
    return false;
  }

  TabStripModel* tabstrip_model = browser->tabstrip_model();
  // If we're not closing all tabs, there's no restriction.
  if (static_cast<int>(indices->size()) != tabstrip_model->count())
    return true;

  // If first tab is NTP, it can't be closed.
  // In TabStripModel::InternalCloseTabs (which calls
  // Browser::CanCloseContents which in turn calls this method), all
  // renderer processes of tabs could be terminated before the tabs are actually
  // closed.
  // As tabs are being closed, notification TabDetachedAt is called.
  // When this happens to the last second tab, we would prevent the last NTP
  // tab from being closed.
  // If we don't prevent this NTP tab from being closed now, its renderer
  // process would have been terminated but the tab won't be detached later,
  // resulting in the "Aw, Snap" page replacing the first NTP.
  // This is the main purpose of this method CanCloseTabs.
  for (size_t i = 0; i < indices->size(); ++i) {
    if ((*indices)[i] == 0) {
      if (tabstrip_model->GetTabContentsAt(0)->web_contents()->GetURL() ==
          GURL(chrome::kChromeUINewTabURL)) {  // First tab is NewTabPage.
        indices->erase(indices->begin() + i);  // Don't close it.
        return false;
      }
      break;
    }
  }
  return true;
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

  // Only tabbed browsers may affect closeable state.
  if (!browser->is_type_tabbed())
    return;

  // Create TabStripWatcher to observe tabstrip of new browser.
  tabstrip_watchers_.push_back(new TabStripWatcher(this, browser));

  // When a normal browser is just added, there's no tabs yet, so we wait till
  // TabInsertedAt notification to check for change in state.
}

void TabCloseableStateWatcher::OnBrowserRemoved(const Browser* browser) {
  // Only tabbed browsers may affect closeable state.
  if (!browser->is_type_tabbed())
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
// TabCloseableStateWatcher, content::NotificationObserver implementation:

void TabCloseableStateWatcher::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != content::NOTIFICATION_APP_EXITING)
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
      (browser_to_check && !browser_to_check->is_type_tabbed()))
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
            tabstrip_model->GetTabContentsAt(0)->web_contents()->GetURL() !=
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
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CLOSEABLE_STATE_CHANGED,
      content::NotificationService::AllSources(),
      content::Details<bool>(&can_close_tab_));
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

  // Non-tabbed browsers are always closeable.
  if (!browser->is_type_tabbed())
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
