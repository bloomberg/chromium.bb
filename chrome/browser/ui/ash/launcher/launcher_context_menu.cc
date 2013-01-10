// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include <string>

#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
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
                                         const ash::LauncherItem* item,
                                         aura::RootWindow* root)
    : ui::SimpleMenuModel(NULL),
      controller_(controller),
      item_(*item),
      launcher_alignment_menu_(root),
      extension_items_(NULL),
      root_window_(root) {
  DCHECK(item);
  DCHECK(root_window_);
  Init();
}

LauncherContextMenu::LauncherContextMenu(ChromeLauncherController* controller,
                                         aura::RootWindow* root)
    : ui::SimpleMenuModel(NULL),
      controller_(controller),
      item_(ash::LauncherItem()),
      launcher_alignment_menu_(root),
      extension_items_(new extensions::ContextMenuMatcher(
          controller->profile(), this, this,
          base::Bind(MenuItemHasLauncherContext))),
      root_window_(root) {
  DCHECK(root_window_);
  Init();
}

void LauncherContextMenu::Init() {
  extension_items_.reset(new extensions::ContextMenuMatcher(
      controller_->profile(), this, this,
      base::Bind(MenuItemHasLauncherContext)));
  set_delegate(this);

  if (is_valid_item()) {
    if (item_.type == ash::TYPE_APP_SHORTCUT) {
      DCHECK(controller_->IsPinned(item_.id));
      // V1 apps can be started from the menu - but V2 apps should not.
      if  (!controller_->IsPlatformApp(item_.id)) {
        AddItem(MENU_OPEN_NEW, string16());
        AddSeparator(ui::NORMAL_SEPARATOR);
      }
      AddItem(
          MENU_PIN,
          l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_UNPIN));
      if (controller_->IsOpen(item_.id)) {
        AddItem(MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
      }
      if (!controller_->IsPlatformApp(item_.id)) {
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
      }
    } else if (item_.type == ash::TYPE_BROWSER_SHORTCUT) {
      AddItem(MENU_NEW_WINDOW,
              l10n_util::GetStringUTF16(IDS_LAUNCHER_NEW_WINDOW));
      if (!controller_->IsLoggedInAsGuest()) {
        AddItem(MENU_NEW_INCOGNITO_WINDOW,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_NEW_INCOGNITO_WINDOW));
      }
    } else {
      if (item_.type == ash::TYPE_PLATFORM_APP) {
        AddItem(
            MENU_PIN,
            l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_PIN));
      }
      if (controller_->IsOpen(item_.id)) {
        AddItem(MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
      }
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    if (item_.type == ash::TYPE_APP_SHORTCUT ||
        item_.type == ash::TYPE_PLATFORM_APP) {
      std::string app_id = controller_->GetAppIDForLauncherID(item_.id);
      if (!app_id.empty()) {
        int index = 0;
        extension_items_->AppendExtensionItems(
            app_id, string16(), &index);
        AddSeparatorIfNecessary(ui::NORMAL_SEPARATOR);
      }
    }
  }
  AddCheckItemWithStringId(
      MENU_AUTO_HIDE, IDS_AURA_LAUNCHER_CONTEXT_MENU_AUTO_HIDE);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowLauncherAlignmentMenu)) {
    AddSubMenuWithStringId(MENU_ALIGNMENT_MENU,
                           IDS_AURA_LAUNCHER_CONTEXT_MENU_POSITION,
                           &launcher_alignment_menu_);
  }
  AddItem(MENU_CHANGE_WALLPAPER,
       l10n_util::GetStringUTF16(IDS_AURA_SET_DESKTOP_WALLPAPER));
}

LauncherContextMenu::~LauncherContextMenu() {
}

bool LauncherContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == MENU_OPEN_NEW;
}

string16 LauncherContextMenu::GetLabelForCommandId(int command_id) const {
  if (command_id == MENU_OPEN_NEW) {
    if (item_.type == ash::TYPE_PLATFORM_APP) {
      return l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_NEW_WINDOW);
    }
    switch (controller_->GetLaunchType(item_.id)) {
      case extensions::ExtensionPrefs::LAUNCH_PINNED:
      case extensions::ExtensionPrefs::LAUNCH_REGULAR:
        return l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_NEW_TAB);
      case extensions::ExtensionPrefs::LAUNCH_FULLSCREEN:
      case extensions::ExtensionPrefs::LAUNCH_WINDOW:
        return l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_NEW_WINDOW);
    }
  }
  NOTREACHED();
  return string16();
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
      return controller_->GetShelfAutoHideBehavior(root_window_) ==
          ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
    default:
      return extension_items_->IsCommandIdChecked(command_id);
  }
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case MENU_PIN:
      return item_.type == ash::TYPE_PLATFORM_APP ||
          controller_->IsPinnable(item_.id);
    case MENU_CHANGE_WALLPAPER:
      return ash::Shell::GetInstance()->user_wallpaper_delegate()->
          CanOpenSetWallpaperPage();
    case MENU_NEW_WINDOW:
      // "Normal" windows are not allowed when incognito is enforced.
      return IncognitoModePrefs::GetAvailability(
          controller_->profile()->GetPrefs()) != IncognitoModePrefs::FORCED;
    case MENU_AUTO_HIDE:
      return controller_->CanUserModifyShelfAutoHideBehavior(root_window_);
    case MENU_NEW_INCOGNITO_WINDOW:
      // Incognito windows are not allowed when incognito is disabled.
      return IncognitoModePrefs::GetAvailability(
          controller_->profile()->GetPrefs()) != IncognitoModePrefs::DISABLED;
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
    case MENU_OPEN_NEW:
      controller_->Launch(item_.id, ui::EF_NONE);
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
      controller_->ToggleShelfAutoHideBehavior(root_window_);
      break;
    case MENU_NEW_WINDOW:
      controller_->CreateNewWindow();
      break;
    case MENU_NEW_INCOGNITO_WINDOW:
      controller_->CreateNewIncognitoWindow();
      break;
    case MENU_ALIGNMENT_MENU:
      break;
    case MENU_CHANGE_WALLPAPER:
      ash::Shell::GetInstance()->user_wallpaper_delegate()->
          OpenSetWallpaperPage();
      break;
    default:
      extension_items_->ExecuteCommand(command_id, NULL,
                                       content::ContextMenuParams());
  }
}
