// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_STATUS_MONITOR_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_STATUS_MONITOR_H_

#include <stdint.h>

#include <map>
#include <string>

#include "ash/shelf/scoped_observer_with_duplicated_sources.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;

namespace client {
class ActivationClient;
}
}  // namespace aura

class Browser;

// BrowserStatusMonitor monitors creation/deletion of Browser and its
// TabStripModel to keep the launcher representation up to date as the
// active tab changes.
class BrowserStatusMonitor : public aura::client::ActivationChangeObserver,
                             public aura::WindowObserver,
                             public BrowserTabStripTrackerDelegate,
                             public chrome::BrowserListObserver,
                             public display::DisplayObserver,
                             public TabStripModelObserver {
 public:
  explicit BrowserStatusMonitor(ChromeLauncherController* launcher_controller);
  ~BrowserStatusMonitor() override;

  // A function which gets called when the current user has changed.
  // Note that this function is called by the ChromeLauncherController to be
  // able to do the activation in a proper order - rather then setting an
  // observer.
  virtual void ActiveUserChanged(const std::string& user_email) {}

  // A shortcut to call the ChromeLauncherController's UpdateAppState().
  void UpdateAppItemState(content::WebContents* contents,
                          ChromeLauncherController::AppState app_state);

  // A shortcut to call the BrowserShortcutLauncherItemController's
  // UpdateBrowserItemState().
  void UpdateBrowserItemState();

  // aura::client::ActivationChangeObserver overrides:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // aura::WindowObserver overrides:
  void OnWindowDestroyed(aura::Window* window) override;

  // BrowserTabStripTrackerDelegate overrides:
  bool ShouldTrackBrowser(Browser* browser) override;

  // chrome::BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // display::DisplayObserver overrides:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // TabStripModelObserver overrides:
  void ActiveTabChanged(content::WebContents* old_contents,
                        content::WebContents* new_contents,
                        int index,
                        int reason) override;
  void TabReplacedAt(TabStripModel* tab_strip_model,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents,
                     int index) override;
  void TabInsertedAt(content::WebContents* contents,
                     int index,
                     bool foreground) override;
  void TabClosingAt(TabStripModel* tab_strip_mode,
                    content::WebContents* contents,
                    int index) override;

  // Called from our own |LocalWebContentsObserver| when web contents did go
  // away without any other notification. This might happen in case of
  // application uninstalls, page crashes, ...).
  void WebContentsDestroyed(content::WebContents* web_contents);

 protected:
  // Add a V1 application to the shelf. This can get overwritten for multi
  // profile implementations.
  virtual void AddV1AppToShelf(Browser* browser);

  // Remove a V1 application from the shelf. This can get overwritten for multi
  // profile implementations.
  virtual void RemoveV1AppFromShelf(Browser* browser);

  // Check if V1 application is currently in the shelf.
  bool IsV1AppInShelf(Browser* browser);

 private:
  class LocalWebContentsObserver;
  class SettingsWindowObserver;

  typedef std::map<Browser*, std::string> BrowserToAppIDMap;
  typedef std::map<content::WebContents*, LocalWebContentsObserver*>
      WebContentsToObserverMap;

  // Create LocalWebContentsObserver for |contents|.
  void AddWebContentsObserver(content::WebContents* contents);

  // Remove LocalWebContentsObserver for |contents|.
  void RemoveWebContentsObserver(content::WebContents* contents);

  // Returns the ShelfID for |contents|.
  ash::ShelfID GetShelfIDForWebContents(content::WebContents* contents);

  // Sets the shelf id for browsers represented by the browser shortcut item.
  void SetShelfIDForBrowserWindowContents(Browser* browser,
                                          content::WebContents* web_contents);

  ChromeLauncherController* launcher_controller_;

  // Hold all observed activation clients.
  ScopedObserverWithDuplicatedSources<aura::client::ActivationClient,
      aura::client::ActivationChangeObserver> observed_activation_clients_;

  // Hold all observed root windows.
  ScopedObserver<aura::Window, aura::WindowObserver> observed_root_windows_;

  BrowserToAppIDMap browser_to_app_id_map_;
  WebContentsToObserverMap webcontents_to_observer_map_;
  std::unique_ptr<SettingsWindowObserver> settings_window_observer_;

  BrowserTabStripTracker browser_tab_strip_tracker_;

  DISALLOW_COPY_AND_ASSIGN(BrowserStatusMonitor);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_STATUS_MONITOR_H_
