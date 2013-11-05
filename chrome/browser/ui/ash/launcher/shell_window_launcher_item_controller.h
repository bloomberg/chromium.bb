// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_

#include <list>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "ui/aura/window_observer.h"

namespace apps {
class ShellWindow;
}

namespace aura {
class Window;
}

namespace gfx {
class Image;
}

class ChromeLauncherController;

// This is a ShellWindowItemLauncherController for shell windows. There is one
// instance per app, per launcher id.
// For apps with multiple windows, each item controller keeps track of all
// windows associated with the app and their activation order.
// Instances are owned by ash::LauncherItemDelegateManager.
//
// Tests are in chrome_launcher_controller_browsertest.cc

class ShellWindowLauncherItemController : public LauncherItemController,
                                          public aura::WindowObserver {
 public:
  ShellWindowLauncherItemController(Type type,
                                    const std::string& app_launcher_id,
                                    const std::string& app_id,
                                    ChromeLauncherController* controller);

  virtual ~ShellWindowLauncherItemController();

  void AddShellWindow(apps::ShellWindow* shell_window,
                      ash::LauncherItemStatus status);

  void RemoveShellWindowForWindow(aura::Window* window);

  void SetActiveWindow(aura::Window* window);

  const std::string& app_launcher_id() const { return app_launcher_id_; }

  // LauncherItemController overrides:
  virtual bool IsCurrentlyShownInWindow(aura::Window* window) const OVERRIDE;
  virtual bool IsOpen() const OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void Launch(ash::LaunchSource source, int event_flags) OVERRIDE;
  virtual bool Activate(ash::LaunchSource source) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual ChromeLauncherAppMenuItems GetApplicationList(
      int event_flags) OVERRIDE;
  virtual bool ItemSelected(const ui::Event& eent) OVERRIDE;
  virtual base::string16 GetTitle() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      aura::Window* root_window) OVERRIDE;
  virtual ash::LauncherMenuModel* CreateApplicationMenu(
      int event_flags) OVERRIDE;
  virtual bool IsDraggable() OVERRIDE;
  virtual bool ShouldShowTooltip() OVERRIDE;

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;

  // Get the number of running applications/incarnations of this.
  size_t shell_window_count() const { return shell_windows_.size(); }

  // Activates the window at position |index|.
  void ActivateIndexedApp(size_t index);

 private:
  typedef std::list<apps::ShellWindow*> ShellWindowList;

  void ShowAndActivateOrMinimize(apps::ShellWindow* shell_window);

  // Activate the given |window_to_show|, or - if already selected - advance to
  // the next window of similar type.
  void ActivateOrAdvanceToNextShellWindow(apps::ShellWindow* window_to_show);

  // List of associated shell windows
  ShellWindowList shell_windows_;

  // Pointer to the most recently active shell window
  apps::ShellWindow* last_active_shell_window_;

  // The launcher id associated with this set of windows. There is one
  // AppLauncherItemController for each |app_launcher_id_|.
  const std::string app_launcher_id_;

  // Scoped list of observed windows (for removal on destruction)
  ScopedObserver<aura::Window, aura::WindowObserver> observed_windows_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
