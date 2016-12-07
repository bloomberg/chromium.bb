// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_EXTENSION_LAUNCHER_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_EXTENSION_LAUNCHER_CONTEXT_MENU_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "extensions/common/constants.h"

class ChromeLauncherControllerImpl;

namespace ash {
struct ShelfItem;
class WmShelf;
}

namespace extensions {
class ContextMenuMatcher;
}

// Context menu shown for an extension item in the shelf.
class ExtensionLauncherContextMenu : public LauncherContextMenu {
 public:
  ExtensionLauncherContextMenu(ChromeLauncherControllerImpl* controller,
                               const ash::ShelfItem* item,
                               ash::WmShelf* wm_shelf);
  ~ExtensionLauncherContextMenu() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void Init();

  // Helpers to get and set the launch type for the extension item.
  extensions::LaunchType GetLaunchType() const;
  void SetLaunchType(extensions::LaunchType launch_type);

  std::unique_ptr<extensions::ContextMenuMatcher> extension_items_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionLauncherContextMenu);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_EXTENSION_LAUNCHER_CONTEXT_MENU_H_
