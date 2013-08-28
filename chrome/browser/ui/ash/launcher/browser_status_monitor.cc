// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"

#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

BrowserStatusMonitor::BrowserStatusMonitor(
    ChromeLauncherController* launcher_controller)
    : launcher_controller_(launcher_controller),
      observed_activation_clients_(this),
      observed_root_windows_(this) {
  DCHECK(launcher_controller_);
  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);

  browser_list->AddObserver(this);

  // This check needs for win7_aura. Without this, all tests in
  // ChromeLauncherController will fail in win7_aura.
  if (ash::Shell::HasInstance()) {
    // We can't assume all RootWindows have the same ActivationClient.
    // Add a RootWindow and its ActivationClient to the observed list.
    ash::Shell::RootWindowList root_windows = ash::Shell::GetAllRootWindows();
    ash::Shell::RootWindowList::const_iterator iter = root_windows.begin();
    for (; iter != root_windows.end(); ++iter) {
      // |observed_activation_clients_| can have the same activation client
      // multiple times - which would be handled by the used
      // |ScopedObserverWithDuplicatedSources|.
      observed_activation_clients_.Add(
          aura::client::GetActivationClient(*iter));
      observed_root_windows_.Add(static_cast<aura::Window*>(*iter));
    }
    ash::Shell::GetInstance()->GetScreen()->AddObserver(this);
  }
}

BrowserStatusMonitor::~BrowserStatusMonitor() {
  // This check needs for win7_aura. Without this, all tests in
  // ChromeLauncherController will fail in win7_aura.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->GetScreen()->RemoveObserver(this);

  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);

  browser_list->RemoveObserver(this);
}

void BrowserStatusMonitor::OnWindowActivated(aura::Window* gained_active,
                                             aura::Window* lost_active) {
  Browser* browser = chrome::FindBrowserWithWindow(lost_active);
  content::WebContents* active_contents = NULL;

  if (browser) {
    active_contents = browser->tab_strip_model()->GetActiveWebContents();
    if (active_contents)
      UpdateAppState(active_contents);
  }

  browser = chrome::FindBrowserWithWindow(gained_active);
  if (browser) {
    active_contents = browser->tab_strip_model()->GetActiveWebContents();
    if (active_contents)
      UpdateAppState(active_contents);
  }
}

void BrowserStatusMonitor::OnWindowDestroyed(aura::Window* window) {
  // Remove RootWindow and its ActivationClient from observed list.
  observed_root_windows_.Remove(window);
  observed_activation_clients_.Remove(aura::client::GetActivationClient(
      static_cast<aura::RootWindow*>(window)));
}

void BrowserStatusMonitor::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);

  if (browser->is_type_popup() && browser->is_app()) {
    std::string app_id =
        web_app::GetExtensionIdFromApplicationName(browser->app_name());
    if (!app_id.empty()) {
      browser_to_app_id_map_[browser] = app_id;
      launcher_controller_->LockV1AppWithID(app_id);
    }
  }
}

void BrowserStatusMonitor::OnBrowserRemoved(Browser* browser) {
  browser->tab_strip_model()->RemoveObserver(this);

  if (browser_to_app_id_map_.find(browser) != browser_to_app_id_map_.end()) {
    launcher_controller_->UnlockV1AppWithID(browser_to_app_id_map_[browser]);
    browser_to_app_id_map_.erase(browser);
  }
  launcher_controller_->UpdateBrowserItemStatus();
}

void BrowserStatusMonitor::OnDisplayBoundsChanged(
    const gfx::Display& display) {
  // Do nothing here.
}

void BrowserStatusMonitor::OnDisplayAdded(const gfx::Display& new_display) {
  // Add a new RootWindow and its ActivationClient to observed list.
  aura::RootWindow* root_window = ash::Shell::GetInstance()->
      display_controller()->GetRootWindowForDisplayId(new_display.id());
  observed_activation_clients_.Add(
      aura::client::GetActivationClient(root_window));
}

void BrowserStatusMonitor::OnDisplayRemoved(const gfx::Display& old_display) {
  // When this is called, RootWindow of |old_display| is already removed.
  // Instead, we can remove RootWindow and its ActivationClient in the
  // OnWindowRemoved().
  // Do nothing here.
}

void BrowserStatusMonitor::ActiveTabChanged(content::WebContents* old_contents,
                                            content::WebContents* new_contents,
                                            int index,
                                            int reason) {
  Browser* browser = NULL;
  if (old_contents)
    browser = chrome::FindBrowserWithWebContents(old_contents);

  // Update immediately on a tab change.
  if (browser &&
      (TabStripModel::kNoTab !=
           browser->tab_strip_model()->GetIndexOfWebContents(old_contents)))
    UpdateAppState(old_contents);

  UpdateAppState(new_contents);
}

void BrowserStatusMonitor::TabInsertedAt(content::WebContents* contents,
                                         int index,
                                         bool foreground) {
  UpdateAppState(contents);
}

void BrowserStatusMonitor::TabDetachedAt(content::WebContents* contents,
                                         int index) {
  launcher_controller_->UpdateAppState(
      contents, ChromeLauncherController::APP_STATE_REMOVED);
}

void BrowserStatusMonitor::TabChangedAt(
    content::WebContents* contents,
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  UpdateAppState(contents);
}

void BrowserStatusMonitor::TabReplacedAt(TabStripModel* tab_strip_model,
                                         content::WebContents* old_contents,
                                         content::WebContents* new_contents,
                                         int index) {
  launcher_controller_->UpdateAppState(
      old_contents,
      ChromeLauncherController::APP_STATE_REMOVED);
  UpdateAppState(new_contents);
}

void BrowserStatusMonitor::UpdateAppState(content::WebContents* contents) {
  if (!contents)
    return;

  ChromeLauncherController::AppState app_state =
      ChromeLauncherController::APP_STATE_INACTIVE;

  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (browser->tab_strip_model()->GetActiveWebContents() == contents) {
    if (browser->window()->IsActive())
      app_state = ChromeLauncherController::APP_STATE_WINDOW_ACTIVE;
    else
      app_state = ChromeLauncherController::APP_STATE_ACTIVE;
  }

  launcher_controller_->UpdateAppState(contents, app_state);
}
