// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_LAUNCHER_DELEGATE_H_
#define ASH_TEST_TEST_LAUNCHER_DELEGATE_H_
#pragma once

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

  static TestLauncherDelegate* instance() { return instance_; }

  // WindowObserver implementation
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;

  // LauncherDelegate implementation.
  virtual void CreateNewTab() OVERRIDE;
  virtual void CreateNewWindow() OVERRIDE;
  virtual void ItemClicked(const LauncherItem& item,
                           int event_flags) OVERRIDE;
  virtual int GetBrowserShortcutResourceId() OVERRIDE;
  virtual string16 GetTitle(const LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(const LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenuForLauncher() OVERRIDE;
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE;
  virtual bool IsDraggable(const ash::LauncherItem& item) OVERRIDE;

 private:
  typedef std::map<aura::Window*, ash::LauncherID> WindowToID;
  typedef std::set<aura::Window*> ObservedWindows;

  static TestLauncherDelegate* instance_;

  aura::Window* GetWindowByID(ash::LauncherID id);

  LauncherModel* model_;

  // Maps from window to the id we gave it.
  WindowToID window_to_id_;

  // Parent windows we are watching.
  ObservedWindows observed_windows_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_LAUNCHER_DELEGATE
