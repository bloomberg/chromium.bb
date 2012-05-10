// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/launcher_context_menu.h"

#include "ash/launcher/launcher_context_menu.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_switches.h"
#include "grit/ash_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LauncherContextMenu::LauncherContextMenu(ChromeLauncherController* controller,
                                         const ash::LauncherItem* item)
    : ui::SimpleMenuModel(NULL),
      controller_(controller),
      item_(item ? *item : ash::LauncherItem()) {
  set_delegate(this);

  if (is_valid_item()) {
    if (item_.type == ash::TYPE_APP_SHORTCUT) {
      DCHECK(controller->IsPinned(item_.id));
      AddItem(
          MENU_PIN,
          l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_UNPIN));
      AddSeparator();
      AddCheckItemWithStringId(
          LAUNCH_TYPE_REGULAR_TAB,
          IDS_APP_CONTEXT_MENU_OPEN_REGULAR);
      AddCheckItemWithStringId(
          LAUNCH_TYPE_PINNED_TAB,
          IDS_APP_CONTEXT_MENU_OPEN_PINNED);
      AddCheckItemWithStringId(
          LAUNCH_TYPE_WINDOW,
          IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
      // Even though the launch type is Full Screen it is more accurately
      // described as Maximized in Ash.
      AddCheckItemWithStringId(
          LAUNCH_TYPE_FULLSCREEN,
          IDS_APP_CONTEXT_MENU_OPEN_MAXIMIZED);
    } else if (item_.type == ash::TYPE_BROWSER_SHORTCUT) {
      AddItem(MENU_NEW_WINDOW,
              l10n_util::GetStringUTF16(IDS_LAUNCHER_NEW_WINDOW));
      if (!controller_->IsLoggedInAsGuest()) {
        AddItem(MENU_NEW_INCOGNITO_WINDOW,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_NEW_INCOGNITO_WINDOW));
      }
    } else {
      AddItem(MENU_OPEN, controller->GetTitle(item_));
      if (controller->IsOpen(item_.id)) {
        AddItem(MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
      }
    }
    AddSeparator();
  }
  AddCheckItemWithStringId(
      MENU_AUTO_HIDE, ash::LauncherContextMenu::GetAutoHideResourceStringId());
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowLauncherAlignmentMenu)) {
    AddSubMenuWithStringId(MENU_ALIGNMENT_MENU,
                           IDS_AURA_LAUNCHER_CONTEXT_MENU_POSITION,
                           &alignment_menu_);
  }
}

LauncherContextMenu::~LauncherContextMenu() {
}

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case LAUNCH_TYPE_PINNED_TAB:
      return controller_->GetLaunchType(item_.id) ==
          ExtensionPrefs::LAUNCH_PINNED;
    case LAUNCH_TYPE_REGULAR_TAB:
      return controller_->GetLaunchType(item_.id) ==
          ExtensionPrefs::LAUNCH_REGULAR;
    case LAUNCH_TYPE_WINDOW:
      return controller_->GetLaunchType(item_.id) ==
          ExtensionPrefs::LAUNCH_WINDOW;
    case LAUNCH_TYPE_FULLSCREEN:
      return controller_->GetLaunchType(item_.id) ==
          ExtensionPrefs::LAUNCH_FULLSCREEN;
    case MENU_AUTO_HIDE:
      return ash::LauncherContextMenu::IsAutoHideMenuHideChecked();
    default:
      return false;
  }
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case MENU_PIN:
      return controller_->IsPinnable(item_.id);
    default:
      return true;
  }
}

bool LauncherContextMenu::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return false;
}

void LauncherContextMenu::ExecuteCommand(int command_id) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_OPEN:
      controller_->Open(item_.id, ui::EF_NONE);
      break;
    case MENU_CLOSE:
      controller_->Close(item_.id);
      break;
    case MENU_PIN:
      controller_->TogglePinned(item_.id);
      break;
    case LAUNCH_TYPE_PINNED_TAB:
      controller_->SetLaunchType(item_.id, ExtensionPrefs::LAUNCH_PINNED);
      break;
    case LAUNCH_TYPE_REGULAR_TAB:
      controller_->SetLaunchType(item_.id, ExtensionPrefs::LAUNCH_REGULAR);
      break;
    case LAUNCH_TYPE_WINDOW:
      controller_->SetLaunchType(item_.id, ExtensionPrefs::LAUNCH_WINDOW);
      break;
    case LAUNCH_TYPE_FULLSCREEN:
      controller_->SetLaunchType(item_.id, ExtensionPrefs::LAUNCH_FULLSCREEN);
      break;
    case MENU_AUTO_HIDE:
      controller_->SetAutoHideBehavior(
          ash::LauncherContextMenu::GetToggledAutoHideBehavior());
      break;
    case MENU_NEW_WINDOW:
      controller_->CreateNewWindow();
      break;
    case MENU_NEW_INCOGNITO_WINDOW:
      controller_->CreateNewIncognitoWindow();
      break;
    case MENU_ALIGNMENT_MENU:
      break;
  }
}
