// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_LAUNCHER_DELEGATE_H_
#define ASH_TEST_TEST_LAUNCHER_DELEGATE_H_

#include <map>
#include <set>

#include "ash/launcher/launcher_delegate.h"
#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"

namespace ash {

class LauncherModel;

namespace test {

// Test implementation of LauncherDelegate.
// Tests may create icons for windows by calling AddLauncherItem
class TestLauncherDelegate : public LauncherDelegate,
                             public aura::WindowObserver {
 public:
  explicit TestLauncherDelegate(LauncherModel* model);
  virtual ~TestLauncherDelegate();

  void AddLauncherItem(aura::Window* window);
  void AddLauncherItem(aura::Window* window, LauncherItemStatus status);
  void RemoveLauncherItemForWindow(aura::Window* window);

  static TestLauncherDelegate* instance() { return instance_; }

  // WindowObserver implementation
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowHierarchyChanging(
      const HierarchyChangeParams& params) OVERRIDE;

  // LauncherDelegate implementation.
  virtual void ItemSelected(const LauncherItem& item,
                           const ui::Event& event) OVERRIDE;
  virtual base::string16 GetTitle(const LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(const LauncherItem& item,
                                           aura::RootWindow* root) OVERRIDE;
  virtual ash::LauncherMenuModel* CreateApplicationMenu(
      const LauncherItem& item,
      int event_flags) OVERRIDE;
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE;
  virtual bool IsDraggable(const ash::LauncherItem& item) OVERRIDE;
  virtual bool ShouldShowTooltip(const LauncherItem& item) OVERRIDE;
  virtual void OnLauncherCreated(Launcher* launcher) OVERRIDE;
  virtual void OnLauncherDestroyed(Launcher* launcher) OVERRIDE;
  virtual bool IsPerAppLauncher() OVERRIDE;
  virtual LauncherID GetLauncherIDForAppID(const std::string& app_id) OVERRIDE;
  virtual void PinAppWithID(const std::string& app_id) OVERRIDE;
  virtual bool IsAppPinned(const std::string& app_id) OVERRIDE;
  virtual void UnpinAppsWithID(const std::string& app_id) OVERRIDE;

 private:
  typedef std::map<aura::Window*, ash::LauncherID> WindowToID;
  typedef std::set<aura::Window*> ObservedWindows;

  static TestLauncherDelegate* instance_;

  aura::Window* GetWindowByID(ash::LauncherID id);

  LauncherModel* model_;

  // Maps from window to the id we gave it.
  WindowToID window_to_id_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_LAUNCHER_DELEGATE_H_
