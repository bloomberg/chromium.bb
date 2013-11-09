// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/multi_profile_browser_status_monitor.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

MultiProfileBrowserStatusMonitor::MultiProfileBrowserStatusMonitor(
    ChromeLauncherController* launcher_controller)
    : BrowserStatusMonitor(launcher_controller),
      launcher_controller_(launcher_controller) {
}

MultiProfileBrowserStatusMonitor::~MultiProfileBrowserStatusMonitor() {
}

void MultiProfileBrowserStatusMonitor::ActiveUserChanged(
    const std::string& user_email) {
  // Handle windowed apps.
  for (AppList::iterator it = app_list_.begin(); it != app_list_.end(); ++it) {
    bool owned = multi_user_util::IsProfileFromActiveUser((*it)->profile());
    bool shown = IsV1AppInShelf(*it);
    if (owned && !shown)
      BrowserStatusMonitor::AddV1AppToShelf(*it);
    else if (!owned && shown)
      BrowserStatusMonitor::RemoveV1AppFromShelf(*it);
  }

  // Handle apps in browser tabs: Add the new applications.
  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);

  // Remove old (tabbed V1) applications.
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end(); ++it) {
    Browser* browser = *it;
    if (!browser->is_app() &&
        browser->is_type_tabbed() &&
        !multi_user_util::IsProfileFromActiveUser(browser->profile())) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        launcher_controller_->UpdateAppState(
            browser->tab_strip_model()->GetWebContentsAt(i),
            ChromeLauncherController::APP_STATE_REMOVED);
      }
    }
  }

  // Handle apps in browser tabs: Add new (tabbed V1) applications.
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end(); ++it) {
    Browser* browser = *it;
    if (!browser->is_app() &&
        browser->is_type_tabbed() &&
        multi_user_util::IsProfileFromActiveUser(browser->profile())) {
      int active_index = browser->tab_strip_model()->active_index();
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        launcher_controller_->UpdateAppState(
            browser->tab_strip_model()->GetWebContentsAt(i),
            browser->window()->IsActive() && i == active_index ?
                ChromeLauncherController::APP_STATE_WINDOW_ACTIVE :
                ChromeLauncherController::APP_STATE_INACTIVE);
      }
    }
  }

  // Update the browser state since some of the removals / adds above might have
  // had an impact on the browser item.
  UpdateBrowserItemState();
}

void MultiProfileBrowserStatusMonitor::AddV1AppToShelf(Browser* browser) {
  DCHECK(browser->is_type_popup() && browser->is_app());
  DCHECK(std::find(app_list_.begin(), app_list_.end(), browser) ==
             app_list_.end());
  app_list_.push_back(browser);
  if (multi_user_util::IsProfileFromActiveUser(browser->profile())) {
    BrowserStatusMonitor::AddV1AppToShelf(browser);
  }
}

void MultiProfileBrowserStatusMonitor::RemoveV1AppFromShelf(Browser* browser) {
  DCHECK(browser->is_type_popup() && browser->is_app());
  AppList::iterator it = std::find(app_list_.begin(), app_list_.end(), browser);
  DCHECK(it != app_list_.end());
  app_list_.erase(it);
  if (multi_user_util::IsProfileFromActiveUser(browser->profile())) {
    BrowserStatusMonitor::RemoveV1AppFromShelf(browser);
  }
}
