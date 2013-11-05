// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_WINDOW_WATCHER_LAUNCHER_ITEM_DELEGATE_
#define ASH_SHELL_WINDOW_WATCHER_LAUNCHER_ITEM_DELEGATE_

#include "ash/launcher/launcher_item_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {
namespace shell {

class WindowWatcher;

// LauncherItemDelegate implementation used by WindowWatcher.
class WindowWatcherLauncherItemDelegate : public ash::LauncherItemDelegate {
 public:
  WindowWatcherLauncherItemDelegate(ash::LauncherID id,
                                    ash::shell::WindowWatcher* watcher);
  virtual ~WindowWatcherLauncherItemDelegate();

  // ash::LauncherItemDelegate overrides:
  virtual bool ItemSelected(const ui::Event& event) OVERRIDE;
  virtual base::string16 GetTitle() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      aura::Window* root_window) OVERRIDE;
  virtual ash::LauncherMenuModel* CreateApplicationMenu(
      int event_flags) OVERRIDE;
  virtual bool IsDraggable() OVERRIDE;
  virtual bool ShouldShowTooltip() OVERRIDE;

 private:
  ash::LauncherID id_;
  ash::shell::WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(WindowWatcherLauncherItemDelegate);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_WINDOW_WATCHER_LAUNCHER_ITEM_DELEGATE_
