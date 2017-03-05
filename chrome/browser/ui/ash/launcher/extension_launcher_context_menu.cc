// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"

#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_impl.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

}  // namespace

ExtensionLauncherContextMenu::ExtensionLauncherContextMenu(
    ChromeLauncherControllerImpl* controller,
    const ash::ShelfItem* item,
    ash::WmShelf* wm_shelf)
    : LauncherContextMenu(controller, item, wm_shelf) {
  Init();
}

ExtensionLauncherContextMenu::~ExtensionLauncherContextMenu() {}

void ExtensionLauncherContextMenu::Init() {
  extension_items_.reset(new extensions::ContextMenuMatcher(
      controller()->profile(), this, this,
      base::Bind(MenuItemHasLauncherContext)));
  if (item().type == ash::TYPE_APP_SHORTCUT || item().type == ash::TYPE_APP) {
    // V1 apps can be started from the menu - but V2 apps should not.
    if (!controller()->IsPlatformApp(item().id)) {
      AddItem(MENU_OPEN_NEW, base::string16());
      AddSeparator(ui::NORMAL_SEPARATOR);
    }

    AddPinMenu();

    if (controller()->IsOpen(item().id))
      AddItemWithStringId(MENU_CLOSE, IDS_LAUNCHER_CONTEXT_MENU_CLOSE);

    if (!controller()->IsPlatformApp(item().id) &&
        item().type == ash::TYPE_APP_SHORTCUT) {
      AddSeparator(ui::NORMAL_SEPARATOR);
      if (extensions::util::IsNewBookmarkAppsEnabled()) {
        // With bookmark apps enabled, hosted apps launch in a window by
        // default. This menu item is re-interpreted as a single, toggle-able
        // option to launch the hosted app as a tab.
        AddCheckItemWithStringId(LAUNCH_TYPE_WINDOW,
                                 IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
      } else {
        AddCheckItemWithStringId(LAUNCH_TYPE_REGULAR_TAB,
                                 IDS_APP_CONTEXT_MENU_OPEN_REGULAR);
        AddCheckItemWithStringId(LAUNCH_TYPE_PINNED_TAB,
                                 IDS_APP_CONTEXT_MENU_OPEN_PINNED);
        AddCheckItemWithStringId(LAUNCH_TYPE_WINDOW,
                                 IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
        // Even though the launch type is Full Screen it is more accurately
        // described as Maximized in Ash.
        AddCheckItemWithStringId(LAUNCH_TYPE_FULLSCREEN,
                                 IDS_APP_CONTEXT_MENU_OPEN_MAXIMIZED);
      }
    }
  } else if (item().type == ash::TYPE_BROWSER_SHORTCUT) {
    AddItemWithStringId(MENU_NEW_WINDOW, IDS_APP_LIST_NEW_WINDOW);
    if (!controller()->profile()->IsGuestSession()) {
      AddItemWithStringId(MENU_NEW_INCOGNITO_WINDOW,
                          IDS_APP_LIST_NEW_INCOGNITO_WINDOW);
    }
    if (!BrowserShortcutLauncherItemController(
             controller(), ash::WmShell::Get()->shelf_model())
             .IsListOfActiveBrowserEmpty()) {
      AddItem(MENU_CLOSE,
              l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
    }
  } else if (item().type == ash::TYPE_DIALOG) {
    AddItemWithStringId(MENU_CLOSE, IDS_LAUNCHER_CONTEXT_MENU_CLOSE);
  } else if (controller()->IsOpen(item().id)) {
    AddItemWithStringId(MENU_CLOSE, IDS_LAUNCHER_CONTEXT_MENU_CLOSE);
  }
  AddSeparator(ui::NORMAL_SEPARATOR);
  if (item().type == ash::TYPE_APP_SHORTCUT || item().type == ash::TYPE_APP) {
    const extensions::MenuItem::ExtensionKey app_key(
        controller()->GetAppIDForShelfID(item().id));
    if (!app_key.empty()) {
      int index = 0;
      extension_items_->AppendExtensionItems(app_key, base::string16(), &index,
                                             false);  // is_action_menu
      AddSeparator(ui::NORMAL_SEPARATOR);
    }
  }
  AddShelfOptionsMenu();
}

bool ExtensionLauncherContextMenu::IsItemForCommandIdDynamic(
    int command_id) const {
  return command_id == MENU_OPEN_NEW;
}

base::string16 ExtensionLauncherContextMenu::GetLabelForCommandId(
    int command_id) const {
  if (command_id == MENU_OPEN_NEW) {
    if (controller()->IsPlatformApp(item().id))
      return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW);
    switch (GetLaunchType()) {
      case extensions::LAUNCH_TYPE_PINNED:
      case extensions::LAUNCH_TYPE_REGULAR:
        return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_TAB);
      case extensions::LAUNCH_TYPE_FULLSCREEN:
      case extensions::LAUNCH_TYPE_WINDOW:
        return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW);
      default:
        NOTREACHED();
        return base::string16();
    }
  }
  NOTREACHED();
  return base::string16();
}

bool ExtensionLauncherContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case LAUNCH_TYPE_PINNED_TAB:
      return GetLaunchType() == extensions::LAUNCH_TYPE_PINNED;
    case LAUNCH_TYPE_REGULAR_TAB:
      return GetLaunchType() == extensions::LAUNCH_TYPE_REGULAR;
    case LAUNCH_TYPE_WINDOW:
      return GetLaunchType() == extensions::LAUNCH_TYPE_WINDOW;
    case LAUNCH_TYPE_FULLSCREEN:
      return GetLaunchType() == extensions::LAUNCH_TYPE_FULLSCREEN;
    default:
      if (command_id < MENU_ITEM_COUNT)
        return LauncherContextMenu::IsCommandIdChecked(command_id);
      return (extension_items_ &&
              extension_items_->IsCommandIdChecked(command_id));
  }
}

bool ExtensionLauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case MENU_NEW_WINDOW:
      // "Normal" windows are not allowed when incognito is enforced.
      return IncognitoModePrefs::GetAvailability(
                 controller()->profile()->GetPrefs()) !=
             IncognitoModePrefs::FORCED;
    case MENU_NEW_INCOGNITO_WINDOW:
      // Incognito windows are not allowed when incognito is disabled.
      return IncognitoModePrefs::GetAvailability(
                 controller()->profile()->GetPrefs()) !=
             IncognitoModePrefs::DISABLED;
    default:
      if (command_id < MENU_ITEM_COUNT)
        return LauncherContextMenu::IsCommandIdEnabled(command_id);
      return (extension_items_ &&
              extension_items_->IsCommandIdEnabled(command_id));
  }
}

void ExtensionLauncherContextMenu::ExecuteCommand(int command_id,
                                                  int event_flags) {
  if (ExecuteCommonCommand(command_id, event_flags))
    return;
  switch (static_cast<MenuItem>(command_id)) {
    case LAUNCH_TYPE_PINNED_TAB:
      SetLaunchType(extensions::LAUNCH_TYPE_PINNED);
      break;
    case LAUNCH_TYPE_REGULAR_TAB:
      SetLaunchType(extensions::LAUNCH_TYPE_REGULAR);
      break;
    case LAUNCH_TYPE_WINDOW: {
      extensions::LaunchType launch_type = extensions::LAUNCH_TYPE_WINDOW;
      // With bookmark apps enabled, hosted apps can only toggle between
      // LAUNCH_WINDOW and LAUNCH_REGULAR.
      if (extensions::util::IsNewBookmarkAppsEnabled()) {
        launch_type = GetLaunchType() == extensions::LAUNCH_TYPE_WINDOW
                          ? extensions::LAUNCH_TYPE_REGULAR
                          : extensions::LAUNCH_TYPE_WINDOW;
      }
      SetLaunchType(launch_type);
      break;
    }
    case LAUNCH_TYPE_FULLSCREEN:
      SetLaunchType(extensions::LAUNCH_TYPE_FULLSCREEN);
      break;
    case MENU_NEW_WINDOW:
      chrome::NewEmptyWindow(controller()->profile());
      break;
    case MENU_NEW_INCOGNITO_WINDOW:
      chrome::NewEmptyWindow(controller()->profile()->GetOffTheRecordProfile());
      break;
    default:
      if (extension_items_) {
        extension_items_->ExecuteCommand(command_id, nullptr, nullptr,
                                         content::ContextMenuParams());
      }
  }
}

extensions::LaunchType ExtensionLauncherContextMenu::GetLaunchType() const {
  const extensions::Extension* extension =
      GetExtensionForAppID(item().app_id, controller()->profile());

  // An extension can be unloaded/updated/unavailable at any time.
  if (!extension)
    return extensions::LAUNCH_TYPE_DEFAULT;

  return extensions::GetLaunchType(
      extensions::ExtensionPrefs::Get(controller()->profile()), extension);
}

void ExtensionLauncherContextMenu::SetLaunchType(extensions::LaunchType type) {
  extensions::SetLaunchType(controller()->profile(), item().app_id, type);
}
