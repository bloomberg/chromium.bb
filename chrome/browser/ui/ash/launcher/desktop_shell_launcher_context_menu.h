// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_DESKTOP_SHELL_LAUNCHER_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_DESKTOP_SHELL_LAUNCHER_CONTEXT_MENU_H_

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

class ChromeLauncherControllerImpl;

namespace ash {
struct ShelfItem;
class WmShelf;
}

// Class for context menu which is shown when right click on desktop shell.
class DesktopShellLauncherContextMenu : public LauncherContextMenu {
 public:
  DesktopShellLauncherContextMenu(ChromeLauncherControllerImpl* controller,
                                  const ash::ShelfItem* item,
                                  ash::WmShelf* shelf);
  ~DesktopShellLauncherContextMenu() override;

 private:
  void Init();

  DISALLOW_COPY_AND_ASSIGN(DesktopShellLauncherContextMenu);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_DESKTOP_SHELL_LAUNCHER_CONTEXT_MENU_H_
