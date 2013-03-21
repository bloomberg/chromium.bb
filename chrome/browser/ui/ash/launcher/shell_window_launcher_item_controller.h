// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_

#include <list>
#include <string>

#include "base/scoped_observer.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace gfx {
class Image;
}

class ChromeLauncherController;
class ShellWindow;

// This is a ShellWindowItemLauncherController for shell windows. There is one
// instance per app, per launcher id.
// For apps with multiple windows, each item controller keeps track of all
// windows associated with the app and their activation order.
// Instances are owned by ShellWindowLauncherController.
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

  void AddShellWindow(ShellWindow* shell_window,
                      ash::LauncherItemStatus status);

  void RemoveShellWindowForWindow(aura::Window* window);

  void SetActiveWindow(aura::Window* window);

  const std::string& app_launcher_id() const { return app_launcher_id_; }

  // LauncherItemController
  virtual string16 GetTitle() OVERRIDE;
  virtual bool HasWindow(aura::Window* window) const OVERRIDE;
  virtual bool IsOpen() const OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void Launch(int event_flags) OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Clicked(const ui::Event& event) OVERRIDE;
  virtual void OnRemoved() OVERRIDE {}
  virtual void LauncherItemChanged(
      int model_index,
      const ash::LauncherItem& old_item) OVERRIDE {}
  virtual ChromeLauncherAppMenuItems GetApplicationList() OVERRIDE;

  // aura::WindowObserver
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;

  // Get the number of running applications/incarnations of this.
  size_t shell_window_count() const { return shell_windows_.size(); }

  // Activates the window at position |index|.
  void ActivateIndexedApp(size_t index);

 private:
  typedef std::list<ShellWindow*> ShellWindowList;

  // Get the title of the running application with this |index|.
  string16 GetTitleOfIndexedApp(size_t index);

  // Get the title of the running application with this |index|.
  // Note that the returned image needs to be released by the caller.
  gfx::Image* GetIconOfIndexedApp(size_t index);

  void RestoreOrShow(ShellWindow* shell_window);

  // List of associated shell windows in activation order.
  ShellWindowList shell_windows_;

  // The launcher id associated with this set of windows. There is one
  // AppLauncherItemController for each |app_launcher_id_|.
  const std::string app_launcher_id_;

  // Scoped list of observed windows (for removal on destruction)
  ScopedObserver<aura::Window, aura::WindowObserver> observed_windows_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
