// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_DELEGATE_H_
#define ASH_LAUNCHER_LAUNCHER_DELEGATE_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"
#include "base/string16.h"

namespace ui {
class MenuModel;
}

namespace ash {

// Delegate for the Launcher.
class ASH_EXPORT LauncherDelegate {
 public:
  virtual ~LauncherDelegate() {}

  // Invoked when the user clicks on button in the launcher to create a new
  // tab.
  virtual void CreateNewTab() = 0;

  // Invoked when the user clicks on button in the launcher to create a new
  // window.
  virtual void CreateNewWindow() = 0;

  // Invoked when the user clicks on a window entry in the launcher.
  // |event_flags| is the flags of the click event.
  virtual void ItemClicked(const LauncherItem& item, int event_flags) = 0;

  // Returns the resource id of the image to show on the browser shortcut
  // button.
  virtual int GetBrowserShortcutResourceId() = 0;

  // Returns the title to display for the specified launcher item.
  virtual string16 GetTitle(const LauncherItem& item) = 0;

  // Returns the context menumodel for the specified item. Return NULL if there
  // should be no context menu. The caller takes ownership of the returned
  // model.
  virtual ui::MenuModel* CreateContextMenu(const LauncherItem& item) = 0;

  // Returns the context menumodel for the launcher. Return NULL if there should
  // be no context menu. The caller takes ownership of the returned model.
  virtual ui::MenuModel* CreateContextMenuForLauncher() = 0;

  // Returns the id of the item associated with the specified window, or 0 if
  // there isn't one.
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) = 0;

  // Whether the given launcher item is draggable.
  virtual bool IsDraggable(const ash::LauncherItem& item) = 0;
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_DELEGATE_H_
