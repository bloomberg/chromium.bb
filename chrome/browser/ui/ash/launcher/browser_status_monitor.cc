// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"

#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager.h"
#include "chrome/browser/ui/settings_window_manager_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/screen.h"
#include "ui/wm/public/activation_client.h"

// This class monitors the WebContent of the all tab and notifies a navigation
// to the BrowserStatusMonitor.
class BrowserStatusMonitor::LocalWebContentsObserver
    : public content::WebContentsObserver {
 public:
  LocalWebContentsObserver(content::WebContents* contents,
                           BrowserStatusMonitor* monitor)
      : content::WebContentsObserver(contents),
        monitor_(monitor) {}

  virtual ~LocalWebContentsObserver() {}

  // content::WebContentsObserver
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    ChromeLauncherController::AppState state =
        ChromeLauncherController::APP_STATE_INACTIVE;
    if (browser->window()->IsActive() &&
        browser->tab_strip_model()->GetActiveWebContents() == web_contents())
      state = ChromeLauncherController::APP_STATE_WINDOW_ACTIVE;
    else if (browser->window()->IsActive())
      state = ChromeLauncherController::APP_STATE_ACTIVE;

    monitor_->UpdateAppItemState(web_contents(), state);
    monitor_->UpdateBrowserItemState();

    // Navigating may change the ShelfID associated with the WebContents.
    if (browser->tab_strip_model()->GetActiveWebContents() == web_contents())
      monitor_->SetShelfIDForBrowserWindowContents(browser, web_contents());
  }

  virtual void WebContentsDestroyed() OVERRIDE {
    // We can only come here when there was a non standard termination like
    // an app got un-installed while running, etc.
    monitor_->WebContentsDestroyed(web_contents());
    // |this| is gone now.
  }

 private:
  BrowserStatusMonitor* monitor_;

  DISALLOW_COPY_AND_ASSIGN(LocalWebContentsObserver);
};

// Observes any new settings windows and sets their shelf icon (since they
// are excluded from BrowserShortcutLauncherItem).
class BrowserStatusMonitor::SettingsWindowObserver
    : public chrome::SettingsWindowManagerObserver {
 public:
  SettingsWindowObserver() {}
  virtual ~SettingsWindowObserver() {}

  // SettingsWindowManagerObserver
  virtual void OnNewSettingsWindow(Browser* settings_browser) OVERRIDE {
    ash::SetShelfItemDetailsForDialogWindow(
        settings_browser->window()->GetNativeWindow(),
        IDR_ASH_SHELF_ICON_SETTINGS);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsWindowObserver);
};

BrowserStatusMonitor::BrowserStatusMonitor(
    ChromeLauncherController* launcher_controller)
    : launcher_controller_(launcher_controller),
      observed_activation_clients_(this),
      observed_root_windows_(this),
      settings_window_observer_(new SettingsWindowObserver) {
  DCHECK(launcher_controller_);
  BrowserList::AddObserver(this);
  chrome::SettingsWindowManager::GetInstance()->AddObserver(
      settings_window_observer_.get());

  // This check needs for win7_aura. Without this, all tests in
  // ChromeLauncherController will fail in win7_aura.
  if (ash::Shell::HasInstance()) {
    // We can't assume all RootWindows have the same ActivationClient.
    // Add a RootWindow and its ActivationClient to the observed list.
    aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
    aura::Window::Windows::const_iterator iter = root_windows.begin();
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

  BrowserList::RemoveObserver(this);

  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  for (BrowserList::const_iterator i = browser_list->begin();
       i != browser_list->end(); ++i) {
    OnBrowserRemoved(*i);
  }

  STLDeleteContainerPairSecondPointers(webcontents_to_observer_map_.begin(),
                                       webcontents_to_observer_map_.end());
}

void BrowserStatusMonitor::UpdateAppItemState(
    content::WebContents* contents,
    ChromeLauncherController::AppState app_state) {
  DCHECK(contents);
  // It is possible to come here from Browser::SwapTabContent where the contents
  // cannot be associated with a browser. A removal however should be properly
  // processed.
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (app_state == ChromeLauncherController::APP_STATE_REMOVED ||
      (browser && launcher_controller_->IsBrowserFromActiveUser(browser)))
    launcher_controller_->UpdateAppState(contents, app_state);
}

void BrowserStatusMonitor::UpdateBrowserItemState() {
  launcher_controller_->GetBrowserShortcutLauncherItemController()->
      UpdateBrowserItemState();
}

void BrowserStatusMonitor::OnWindowActivated(aura::Window* gained_active,
                                             aura::Window* lost_active) {
  Browser* browser = NULL;
  content::WebContents* contents_from_gained = NULL;
  content::WebContents* contents_from_lost = NULL;
  // Update active webcontents's app item state of |lost_active|, if existed.
  if (lost_active) {
    browser = chrome::FindBrowserWithWindow(lost_active);
    if (browser)
      contents_from_lost = browser->tab_strip_model()->GetActiveWebContents();
    if (contents_from_lost) {
      UpdateAppItemState(
          contents_from_lost,
          ChromeLauncherController::APP_STATE_INACTIVE);
    }
  }

  // Update active webcontents's app item state of |gained_active|, if existed.
  if (gained_active) {
    browser = chrome::FindBrowserWithWindow(gained_active);
    if (browser)
      contents_from_gained = browser->tab_strip_model()->GetActiveWebContents();
    if (contents_from_gained) {
      UpdateAppItemState(
          contents_from_gained,
          ChromeLauncherController::APP_STATE_WINDOW_ACTIVE);
    }
  }

  if (contents_from_lost || contents_from_gained)
    UpdateBrowserItemState();
}

void BrowserStatusMonitor::OnWindowDestroyed(aura::Window* window) {
  // Remove RootWindow and its ActivationClient from observed list.
  observed_root_windows_.Remove(window);
  observed_activation_clients_.Remove(
      aura::client::GetActivationClient(window));
}

void BrowserStatusMonitor::OnBrowserAdded(Browser* browser) {
  if (browser->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH)
    return;

  if (browser->is_type_popup() && browser->is_app()) {
    // Note: A V1 application will set the tab strip observer when the app gets
    // added to the shelf. This makes sure that in the multi user case we will
    // only set the observer while the app item exists in the shelf.
    AddV1AppToShelf(browser);
  } else {
    browser->tab_strip_model()->AddObserver(this);
  }
}

void BrowserStatusMonitor::OnBrowserRemoved(Browser* browser) {
  if (browser->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH)
    return;

  if (browser->is_type_popup() && browser->is_app())
    RemoveV1AppFromShelf(browser);
  else
    browser->tab_strip_model()->RemoveObserver(this);

  UpdateBrowserItemState();
}

void BrowserStatusMonitor::OnDisplayAdded(const gfx::Display& new_display) {
  // Add a new RootWindow and its ActivationClient to observed list.
  aura::Window* root_window = ash::Shell::GetInstance()->
      display_controller()->GetRootWindowForDisplayId(new_display.id());
  // When the primary root window's display get removed, the existing root
  // window is taken over by the new display and the observer is already set.
  if (!observed_root_windows_.IsObserving(root_window)) {
    observed_root_windows_.Add(static_cast<aura::Window*>(root_window));
    observed_activation_clients_.Add(
        aura::client::GetActivationClient(root_window));
  }
}

void BrowserStatusMonitor::OnDisplayRemoved(const gfx::Display& old_display) {
  // When this is called, RootWindow of |old_display| is already removed.
  // Instead, we can remove RootWindow and its ActivationClient in the
  // OnWindowRemoved().
  // Do nothing here.
}

void BrowserStatusMonitor::OnDisplayMetricsChanged(const gfx::Display&,
                                                   uint32_t) {
  // Do nothing here.
}

void BrowserStatusMonitor::ActiveTabChanged(content::WebContents* old_contents,
                                            content::WebContents* new_contents,
                                            int index,
                                            int reason) {
  Browser* browser = NULL;
  // Use |new_contents|. |old_contents| could be NULL.
  DCHECK(new_contents);
  browser = chrome::FindBrowserWithWebContents(new_contents);

  if (browser && browser->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH)
    return;

  ChromeLauncherController::AppState state =
      ChromeLauncherController::APP_STATE_INACTIVE;

  // Update immediately on a tab change.
  if (old_contents &&
      (TabStripModel::kNoTab !=
           browser->tab_strip_model()->GetIndexOfWebContents(old_contents)))
    UpdateAppItemState(old_contents, state);

  if (new_contents) {
    state = browser->window()->IsActive() ?
        ChromeLauncherController::APP_STATE_WINDOW_ACTIVE :
        ChromeLauncherController::APP_STATE_ACTIVE;
    UpdateAppItemState(new_contents, state);
    UpdateBrowserItemState();
    SetShelfIDForBrowserWindowContents(browser, new_contents);
  }
}

void BrowserStatusMonitor::TabReplacedAt(TabStripModel* tab_strip_model,
                                         content::WebContents* old_contents,
                                         content::WebContents* new_contents,
                                         int index) {
  DCHECK(old_contents && new_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(new_contents);

  if (browser && browser->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH)
    return;

  UpdateAppItemState(old_contents,
                     ChromeLauncherController::APP_STATE_REMOVED);
  RemoveWebContentsObserver(old_contents);

  ChromeLauncherController::AppState state =
      ChromeLauncherController::APP_STATE_ACTIVE;
  if (browser->window()->IsActive() &&
      (tab_strip_model->GetActiveWebContents() == new_contents))
    state = ChromeLauncherController::APP_STATE_WINDOW_ACTIVE;
  UpdateAppItemState(new_contents, state);
  UpdateBrowserItemState();

  if (tab_strip_model->GetActiveWebContents() == new_contents)
    SetShelfIDForBrowserWindowContents(browser, new_contents);

  AddWebContentsObserver(new_contents);
}

void BrowserStatusMonitor::TabInsertedAt(content::WebContents* contents,
                                         int index,
                                         bool foreground) {
  // An inserted tab is not active - ActiveTabChanged() will be called to
  // activate. We initialize therefore with |APP_STATE_INACTIVE|.
  UpdateAppItemState(contents,
                     ChromeLauncherController::APP_STATE_INACTIVE);
  AddWebContentsObserver(contents);
}

void BrowserStatusMonitor::TabClosingAt(TabStripModel* tab_strip_mode,
                                        content::WebContents* contents,
                                        int index) {
  UpdateAppItemState(contents,
                     ChromeLauncherController::APP_STATE_REMOVED);
  RemoveWebContentsObserver(contents);
}

void BrowserStatusMonitor::WebContentsDestroyed(
    content::WebContents* contents) {
  UpdateAppItemState(contents, ChromeLauncherController::APP_STATE_REMOVED);
  RemoveWebContentsObserver(contents);
}

void BrowserStatusMonitor::AddV1AppToShelf(Browser* browser) {
  DCHECK(browser->is_type_popup() && browser->is_app());

  browser->tab_strip_model()->AddObserver(this);

  std::string app_id =
      web_app::GetExtensionIdFromApplicationName(browser->app_name());
  if (!app_id.empty()) {
    browser_to_app_id_map_[browser] = app_id;
    launcher_controller_->LockV1AppWithID(app_id);
  }
}

void BrowserStatusMonitor::RemoveV1AppFromShelf(Browser* browser) {
  DCHECK(browser->is_type_popup() && browser->is_app());

  browser->tab_strip_model()->RemoveObserver(this);

  if (browser_to_app_id_map_.find(browser) != browser_to_app_id_map_.end()) {
    launcher_controller_->UnlockV1AppWithID(browser_to_app_id_map_[browser]);
    browser_to_app_id_map_.erase(browser);
  }
}

bool BrowserStatusMonitor::IsV1AppInShelf(Browser* browser) {
  return browser_to_app_id_map_.find(browser) != browser_to_app_id_map_.end();
}

void BrowserStatusMonitor::AddWebContentsObserver(
    content::WebContents* contents) {
  if (webcontents_to_observer_map_.find(contents) ==
          webcontents_to_observer_map_.end()) {
    webcontents_to_observer_map_[contents] =
        new LocalWebContentsObserver(contents, this);
  }
}

void BrowserStatusMonitor::RemoveWebContentsObserver(
    content::WebContents* contents) {
  DCHECK(webcontents_to_observer_map_.find(contents) !=
      webcontents_to_observer_map_.end());
  delete webcontents_to_observer_map_[contents];
  webcontents_to_observer_map_.erase(contents);
}

ash::ShelfID BrowserStatusMonitor::GetShelfIDForWebContents(
    content::WebContents* contents) {
  return launcher_controller_->GetShelfIDForWebContents(contents);
}

void BrowserStatusMonitor::SetShelfIDForBrowserWindowContents(
    Browser* browser,
    content::WebContents* web_contents) {
  launcher_controller_->GetBrowserShortcutLauncherItemController()->
      SetShelfIDForBrowserWindowContents(browser, web_contents);
}
