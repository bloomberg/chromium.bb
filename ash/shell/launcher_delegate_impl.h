// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_LAUNCHER_DELEGATE_IMPL_H_
#define ASH_SHELL_LAUNCHER_DELEGATE_IMPL_H_

#include "ash/launcher/launcher_delegate.h"
#include "base/compiler_specific.h"

namespace aura {
class Window;
}

namespace ash {
namespace shell {

class WindowWatcher;

class LauncherDelegateImpl : public ash::LauncherDelegate {
 public:
  explicit LauncherDelegateImpl(WindowWatcher* watcher);
  virtual ~LauncherDelegateImpl();

  void set_watcher(WindowWatcher* watcher) { watcher_ = watcher; }

  // LauncherDelegate overrides:
  virtual void OnBrowserShortcutClicked(int event_flags) OVERRIDE;
  virtual void ItemClicked(const ash::LauncherItem& item,
                           const ui::Event& event) OVERRIDE;
  virtual int GetBrowserShortcutResourceId() OVERRIDE;
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      const ash::LauncherItem& item,
      aura::RootWindow* root) OVERRIDE;
  virtual ash::LauncherMenuModel* CreateApplicationMenu(
      const ash::LauncherItem&,
      int event_flags) OVERRIDE;
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE;
  virtual bool IsDraggable(const ash::LauncherItem& item) OVERRIDE;
  virtual bool ShouldShowTooltip(const LauncherItem& item) OVERRIDE;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(LauncherDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_LAUNCHER_DELEGATE_IMPL_H_
