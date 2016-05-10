// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_item_delegate.h"
#include "base/bind.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/context_menu_params.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

}  // namespace

ExtensionLauncherContextMenu::ExtensionLauncherContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    ash::Shelf* shelf)
    : LauncherContextMenu(controller, item, shelf) {
  Init();
}

ExtensionLauncherContextMenu::~ExtensionLauncherContextMenu() {}

void ExtensionLauncherContextMenu::Init() {
  extension_items_.reset(new extensions::ContextMenuMatcher(
      controller()->profile(), this, this,
      base::Bind(MenuItemHasLauncherContext)));
  if (item().type == ash::TYPE_APP_SHORTCUT ||
      item().type == ash::TYPE_WINDOWED_APP) {
    // V1 apps can be started from the menu - but V2 apps should not.
    if (!controller()->IsPlatformApp(item().id)) {
      AddItem(MENU_OPEN_NEW, base::string16());
      AddSeparator(ui::NORMAL_SEPARATOR);
    }
    AddPinMenu();
    if (controller()->IsOpen(item().id))
      AddItemWithStringId(MENU_CLOSE, IDS_LAUNCHER_CONTEXT_MENU_CLOSE);

    if (!controller()->IsPlatformApp(item().id) &&
        item().type != ash::TYPE_WINDOWED_APP) {
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
    if (!controller()->IsLoggedInAsGuest()) {
      AddItemWithStringId(MENU_NEW_INCOGNITO_WINDOW,
                          IDS_APP_LIST_NEW_INCOGNITO_WINDOW);
    }
  } else if (item().type == ash::TYPE_DIALOG) {
    AddItemWithStringId(MENU_CLOSE, IDS_LAUNCHER_CONTEXT_MENU_CLOSE);
  } else {
    if (item().type == ash::TYPE_PLATFORM_APP)
      AddPinMenu();
    bool show_close_button = controller()->IsOpen(item().id);
    if (extension_misc::IsImeMenuExtensionId(
            controller()->GetAppIDForShelfID(item().id))) {
      show_close_button = false;
    }

    if (show_close_button)
      AddItemWithStringId(MENU_CLOSE, IDS_LAUNCHER_CONTEXT_MENU_CLOSE);
  }
  AddSeparator(ui::NORMAL_SEPARATOR);
  if (item().type == ash::TYPE_APP_SHORTCUT ||
      item().type == ash::TYPE_WINDOWED_APP ||
      item().type == ash::TYPE_PLATFORM_APP) {
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
    if (item().type == ash::TYPE_PLATFORM_APP)
      return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW);
    switch (controller()->GetLaunchType(item().id)) {
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
      return controller()->GetLaunchType(item().id) ==
             extensions::LAUNCH_TYPE_PINNED;
    case LAUNCH_TYPE_REGULAR_TAB:
      return controller()->GetLaunchType(item().id) ==
             extensions::LAUNCH_TYPE_REGULAR;
    case LAUNCH_TYPE_WINDOW:
      return controller()->GetLaunchType(item().id) ==
             extensions::LAUNCH_TYPE_WINDOW;
    case LAUNCH_TYPE_FULLSCREEN:
      return controller()->GetLaunchType(item().id) ==
             extensions::LAUNCH_TYPE_FULLSCREEN;
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
      controller()->SetLaunchType(item().id, extensions::LAUNCH_TYPE_PINNED);
      break;
    case LAUNCH_TYPE_REGULAR_TAB:
      controller()->SetLaunchType(item().id, extensions::LAUNCH_TYPE_REGULAR);
      break;
    case LAUNCH_TYPE_WINDOW: {
      extensions::LaunchType launch_type = extensions::LAUNCH_TYPE_WINDOW;
      // With bookmark apps enabled, hosted apps can only toggle between
      // LAUNCH_WINDOW and LAUNCH_REGULAR.
      if (extensions::util::IsNewBookmarkAppsEnabled()) {
        launch_type = controller()->GetLaunchType(item().id) ==
                              extensions::LAUNCH_TYPE_WINDOW
                          ? extensions::LAUNCH_TYPE_REGULAR
                          : extensions::LAUNCH_TYPE_WINDOW;
      }
      controller()->SetLaunchType(item().id, launch_type);
      break;
    }
    case LAUNCH_TYPE_FULLSCREEN:
      controller()->SetLaunchType(item().id,
                                  extensions::LAUNCH_TYPE_FULLSCREEN);
      break;
    case MENU_NEW_WINDOW:
      controller()->CreateNewWindow();
      break;
    case MENU_NEW_INCOGNITO_WINDOW:
      controller()->CreateNewIncognitoWindow();
      break;
    default:
      if (extension_items_) {
        extension_items_->ExecuteCommand(command_id, nullptr, nullptr,
                                         content::ContextMenuParams());
      }
  }
}
