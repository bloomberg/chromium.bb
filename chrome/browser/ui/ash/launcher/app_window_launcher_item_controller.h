// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_

#include <list>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ui {
class BaseWindow;
}

class ChromeLauncherController;

// This is a LauncherItemController for abstract app windows. There is one
// instance per app, per launcher id. For apps with multiple windows, each item
// controller keeps track of all windows associated with the app and their
// activation order. Instances are owned by ash::ShelfItemDelegateManager.
//
// Tests are in chrome_launcher_controller_browsertest.cc
class AppWindowLauncherItemController : public LauncherItemController,
                                        public aura::WindowObserver {
 public:
  ~AppWindowLauncherItemController() override;

  void AddWindow(ui::BaseWindow* window);
  void RemoveWindow(ui::BaseWindow* window);

  void SetActiveWindow(aura::Window* window);
  ui::BaseWindow* GetAppWindow(aura::Window* window);

  const std::string& app_shelf_id() const { return app_shelf_id_; }

  // LauncherItemController overrides:
  bool IsOpen() const override;
  bool IsVisible() const override;
  void Launch(ash::LaunchSource source, int event_flags) override;
  ash::ShelfItemDelegate::PerformedAction Activate(
      ash::LaunchSource source) override;
  ChromeLauncherAppMenuItems GetApplicationList(int event_flags) override;
  ash::ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& eent) override;
  base::string16 GetTitle() override;
  bool IsDraggable() override;
  bool CanPin() const override;
  bool ShouldShowTooltip() override;
  void Close() override;

  // aura::WindowObserver overrides:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // Get the number of running applications/incarnations of this.
  size_t window_count() const { return windows_.size(); }

  // Activates the window at position |index|.
  void ActivateIndexedApp(size_t index);

 protected:
  using WindowList = std::list<ui::BaseWindow*>;

  AppWindowLauncherItemController(Type type,
                                  const std::string& app_shelf_id,
                                  const std::string& app_id,
                                  ChromeLauncherController* controller);

  // Called when app window is removed from controller.
  virtual void OnWindowRemoved(ui::BaseWindow* window) {}

  // Returns the action performed. Should be one of kNoAction,
  // kExistingWindowActivated, or kExistingWindowMinimized.
  ash::ShelfItemDelegate::PerformedAction ShowAndActivateOrMinimize(
      ui::BaseWindow* window);

  // Activate the given |window_to_show|, or - if already selected - advance to
  // the next window of similar type.
  // Returns the action performed. Should be one of kNoAction,
  // kExistingWindowActivated, or kExistingWindowMinimized.
  ash::ShelfItemDelegate::PerformedAction ActivateOrAdvanceToNextAppWindow(
      ui::BaseWindow* window_to_show);

  const WindowList& windows() { return windows_; }

 private:
  WindowList::iterator GetFromNativeWindow(aura::Window* window);

  // List of associated app windows
  WindowList windows_;

  // Pointer to the most recently active app window
  ui::BaseWindow* last_active_window_ = nullptr;

  // The launcher id associated with this set of windows. There is one
  // AppLauncherItemController for each |app_shelf_id_|.
  const std::string app_shelf_id_;

  // Scoped list of observed windows (for removal on destruction)
  ScopedObserver<aura::Window, aura::WindowObserver> observed_windows_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
