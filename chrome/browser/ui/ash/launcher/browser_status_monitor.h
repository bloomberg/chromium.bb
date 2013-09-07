// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_STATUS_MONITOR_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_STATUS_MONITOR_H_

#include <map>
#include <string>

#include "ash/shelf/scoped_observer_with_duplicated_sources.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/display_observer.h"

namespace aura {
class Window;

namespace client {
class ActivationClient;
}
}  // namespace aura

class Browser;
class ChromeLauncherController;

// BrowserStatusMonitor monitors creation/deletion of Browser and its
// TabStripModel to keep the launcher representation up to date as the
// active tab changes.
class BrowserStatusMonitor : public aura::client::ActivationChangeObserver,
                             public aura::WindowObserver,
                             public chrome::BrowserListObserver,
                             public gfx::DisplayObserver,
                             public TabStripModelObserver {
 public:
  explicit BrowserStatusMonitor(ChromeLauncherController* launcher_controller);
  virtual ~BrowserStatusMonitor();

  // aura::client::ActivationChangeObserver overrides:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // chrome::BrowserListObserver overrides:
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // gfx::DisplayObserver overrides:
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;

  // TabStripModelObserver overrides:
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                int reason) OVERRIDE;
  virtual void TabInsertedAt(content::WebContents* contents,
                             int index,
                             bool foreground) OVERRIDE;
  virtual void TabDetachedAt(content::WebContents* contents,
                             int index) OVERRIDE;
  virtual void TabChangedAt(
      content::WebContents* contents,
      int index,
      TabStripModelObserver::TabChangeType change_type) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index) OVERRIDE;

 private:
  typedef std::map<Browser*, std::string> BrowserToAppIDMap;

  // Update app state for |contents| and browser state.
  void UpdateAppAndBrowserState(content::WebContents* contents);

  // A shortcut to call the BrowserShortcutLauncherItemController's
  // UpdateBrowserItemState().
  void UpdateBrowserItemState();

  ChromeLauncherController* launcher_controller_;

  // Hold all observed activation clients.
  ScopedObserverWithDuplicatedSources<aura::client::ActivationClient,
      aura::client::ActivationChangeObserver> observed_activation_clients_;

  // Hold all observed root windows.
  ScopedObserver<aura::Window, aura::WindowObserver> observed_root_windows_;

  BrowserToAppIDMap browser_to_app_id_map_;

  DISALLOW_COPY_AND_ASSIGN(BrowserStatusMonitor);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_STATUS_MONITOR_H_
