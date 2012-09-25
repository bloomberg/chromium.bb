// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include "ash/launcher/launcher_context_menu.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/context_menu_params.h"
#include "grit/ash_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

}  // namespace

LauncherContextMenu::LauncherContextMenu(ChromeLauncherController* controller,
                                         const ash::LauncherItem* item)
    : ui::SimpleMenuModel(NULL),
      controller_(controller),
      item_(item ? *item : ash::LauncherItem()),
      extension_items_(new extensions::ContextMenuMatcher(
          controller->profile(), this, this,
          base::Bind(MenuItemHasLauncherContext))) {
  set_delegate(this);

  if (is_valid_item()) {
    if (item_.type == ash::TYPE_APP_SHORTCUT) {
      DCHECK(controller->IsPinned(item_.id));
      AddItem(
          MENU_PIN,
          l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_UNPIN));
      if (controller->IsOpen(item->id)) {
        AddItem(MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
      }
      AddSeparator(ui::NORMAL_SEPARATOR);
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
      if (item_.type == ash::TYPE_PLATFORM_APP) {
        AddItem(
            MENU_PIN,
            l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_PIN));
      }
      if (controller->IsOpen(item_.id)) {
        AddItem(MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
      }
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
  }
  std::string app_id = controller->GetAppIDForLauncherID(item_.id);
  if (!app_id.empty()) {
    int index = 0;
    extension_items_->AppendExtensionItems(
        app_id, string16(), &index);
    if (index > 0)
      AddSeparator(ui::NORMAL_SEPARATOR);
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
          extensions::ExtensionPrefs::LAUNCH_PINNED;
    case LAUNCH_TYPE_REGULAR_TAB:
      return controller_->GetLaunchType(item_.id) ==
          extensions::ExtensionPrefs::LAUNCH_REGULAR;
    case LAUNCH_TYPE_WINDOW:
      return controller_->GetLaunchType(item_.id) ==
          extensions::ExtensionPrefs::LAUNCH_WINDOW;
    case LAUNCH_TYPE_FULLSCREEN:
      return controller_->GetLaunchType(item_.id) ==
          extensions::ExtensionPrefs::LAUNCH_FULLSCREEN;
    case MENU_AUTO_HIDE:
      return ash::LauncherContextMenu::IsAutoHideMenuHideChecked();
    default:
      return extension_items_->IsCommandIdChecked(command_id);
  }
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case MENU_PIN:
      return item_.type == ash::TYPE_PLATFORM_APP ||
          controller_->IsPinnable(item_.id);
    default:
      return extension_items_->IsCommandIdEnabled(command_id);
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
      controller_->SetLaunchType(item_.id,
                                 extensions::ExtensionPrefs::LAUNCH_PINNED);
      break;
    case LAUNCH_TYPE_REGULAR_TAB:
      controller_->SetLaunchType(item_.id,
                                 extensions::ExtensionPrefs::LAUNCH_REGULAR);
      break;
    case LAUNCH_TYPE_WINDOW:
      controller_->SetLaunchType(item_.id,
                                 extensions::ExtensionPrefs::LAUNCH_WINDOW);
      break;
    case LAUNCH_TYPE_FULLSCREEN:
      controller_->SetLaunchType(item_.id,
                                 extensions::ExtensionPrefs::LAUNCH_FULLSCREEN);
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
    default:
      extension_items_->ExecuteCommand(command_id,
                                       content::ContextMenuParams());
  }
}
