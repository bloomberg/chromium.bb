// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_context_menu.h"

#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/context_menu_params.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {

namespace {

bool disable_installed_extension_check_for_testing = false;

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

}  // namespace

// static
void ExtensionAppContextMenu::DisableInstalledExtensionCheckForTesting(
    bool disable) {
  disable_installed_extension_check_for_testing = disable;
}

ExtensionAppContextMenu::ExtensionAppContextMenu(
    AppContextMenuDelegate* delegate,
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller)
    : AppContextMenu(delegate, profile, app_id, controller) {
}

ExtensionAppContextMenu::~ExtensionAppContextMenu() {
}

ui::MenuModel* ExtensionAppContextMenu::GetMenuModel() {
  if (!disable_installed_extension_check_for_testing &&
      !controller()->IsExtensionInstalled(profile(), app_id())) {
    return nullptr;
  }

  return AppContextMenu::GetMenuModel();
}

void ExtensionAppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  if (app_id() == extension_misc::kChromeAppId) {
    menu_model->AddItemWithStringId(
        MENU_NEW_WINDOW,
        IDS_APP_LIST_NEW_WINDOW);
    if (!profile()->IsOffTheRecord()) {
      menu_model->AddItemWithStringId(
          MENU_NEW_INCOGNITO_WINDOW,
          IDS_APP_LIST_NEW_INCOGNITO_WINDOW);
    }
    if (controller()->CanDoShowAppInfoFlow()) {
      menu_model->AddItemWithStringId(SHOW_APP_INFO,
                                      IDS_APP_CONTEXT_MENU_SHOW_INFO);
    }
  } else {
    extension_menu_items_.reset(new extensions::ContextMenuMatcher(
        profile(), this, menu_model,
        base::Bind(MenuItemHasLauncherContext)));

    // First, add the primary actions.
    if (!is_platform_app_)
      menu_model->AddItem(LAUNCH_NEW, base::string16());

    // Create default items.
    AppContextMenu::BuildMenu(menu_model);
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);

    if (!is_platform_app_) {
      // When bookmark apps are enabled, hosted apps can only toggle between
      // USE_LAUNCH_TYPE_WINDOW and USE_LAUNCH_TYPE_REGULAR.
      if (extensions::util::CanHostedAppsOpenInWindows() &&
          extensions::util::IsNewBookmarkAppsEnabled()) {
        // When both flags are enabled, only allow toggling between
        // USE_LAUNCH_TYPE_WINDOW and USE_LAUNCH_TYPE_REGULAR
        menu_model->AddCheckItemWithStringId(
            USE_LAUNCH_TYPE_WINDOW, IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
      } else if (!extensions::util::IsNewBookmarkAppsEnabled()) {
        // When new bookmark apps are disabled, add pinned and full screen
        // options as well. Add open as window if CanHostedAppsOpenInWindows
        // is enabled.
        menu_model->AddCheckItemWithStringId(
            USE_LAUNCH_TYPE_REGULAR,
            IDS_APP_CONTEXT_MENU_OPEN_REGULAR);
        menu_model->AddCheckItemWithStringId(
            USE_LAUNCH_TYPE_PINNED,
            IDS_APP_CONTEXT_MENU_OPEN_PINNED);
        if (extensions::util::CanHostedAppsOpenInWindows()) {
          menu_model->AddCheckItemWithStringId(
              USE_LAUNCH_TYPE_WINDOW,
              IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
        }
        // Even though the launch type is Full Screen it is more accurately
        // described as Maximized in Ash.
        menu_model->AddCheckItemWithStringId(
            USE_LAUNCH_TYPE_FULLSCREEN,
            IDS_APP_CONTEXT_MENU_OPEN_MAXIMIZED);
      }
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    }

    // Assign unique IDs to commands added by the app itself.
    int index = USE_LAUNCH_TYPE_COMMAND_END;
    extension_menu_items_->AppendExtensionItems(
        extensions::MenuItem::ExtensionKey(app_id()),
        base::string16(),
        &index,
        false);  // is_action_menu

    // If at least 1 item was added, add another separator after the list.
    if (index > USE_LAUNCH_TYPE_COMMAND_END)
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);

    if (!is_platform_app_)
      menu_model->AddItemWithStringId(OPTIONS, IDS_NEW_TAB_APP_OPTIONS);

    menu_model->AddItemWithStringId(UNINSTALL,
                                    is_platform_app_
                                        ? IDS_APP_LIST_UNINSTALL_ITEM
                                        : IDS_APP_LIST_EXTENSIONS_UNINSTALL);

    if (controller()->CanDoShowAppInfoFlow()) {
      menu_model->AddItemWithStringId(SHOW_APP_INFO,
                                      IDS_APP_CONTEXT_MENU_SHOW_INFO);
    }
  }
}

bool ExtensionAppContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == LAUNCH_NEW ||
      AppContextMenu::IsItemForCommandIdDynamic(command_id);
}

base::string16 ExtensionAppContextMenu::GetLabelForCommandId(
    int command_id) const {
  if (command_id == TOGGLE_PIN)
    return AppContextMenu::GetLabelForCommandId(command_id);

  DCHECK_EQ(LAUNCH_NEW, command_id);

  // If --enable-new-bookmark-apps is enabled, then only check if
  // USE_LAUNCH_TYPE_WINDOW is checked, as USE_LAUNCH_TYPE_PINNED (i.e. open
  // as pinned tab) and fullscreen-by-default windows do not exist.
  bool launches_in_window =
      (extensions::util::IsNewBookmarkAppsEnabled()
           ? IsCommandIdChecked(USE_LAUNCH_TYPE_WINDOW)
           : !(IsCommandIdChecked(USE_LAUNCH_TYPE_PINNED) ||
               IsCommandIdChecked(USE_LAUNCH_TYPE_REGULAR)));

  return launches_in_window ?
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW) :
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_TAB);
}

bool ExtensionAppContextMenu::IsCommandIdChecked(int command_id) const {
  if (command_id >= USE_LAUNCH_TYPE_COMMAND_START &&
      command_id < USE_LAUNCH_TYPE_COMMAND_END) {
    return static_cast<int>(controller()->GetExtensionLaunchType(
        profile(), app_id())) + USE_LAUNCH_TYPE_COMMAND_START == command_id;
  } else if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
                 command_id)) {
    return extension_menu_items_->IsCommandIdChecked(command_id);
  }
  return AppContextMenu::IsCommandIdChecked(command_id);
}

bool ExtensionAppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == OPTIONS) {
    return controller()->HasOptionsPage(profile(), app_id());
  } else if (command_id == UNINSTALL) {
    return controller()->UserMayModifySettings(profile(), app_id());
  } else if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
                 command_id)) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  } else if (command_id == MENU_NEW_WINDOW) {
    // "Normal" windows are not allowed when incognito is enforced.
    return IncognitoModePrefs::GetAvailability(profile()->GetPrefs()) !=
        IncognitoModePrefs::FORCED;
  } else if (command_id == MENU_NEW_INCOGNITO_WINDOW) {
    // Incognito windows are not allowed when incognito is disabled.
    return IncognitoModePrefs::GetAvailability(profile()->GetPrefs()) !=
        IncognitoModePrefs::DISABLED;
  }
  return AppContextMenu::IsCommandIdEnabled(command_id);
}

void ExtensionAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == LAUNCH_NEW) {
    delegate()->ExecuteLaunchCommand(event_flags);
  } else if (command_id == SHOW_APP_INFO) {
    controller()->DoShowAppInfoFlow(profile(), app_id());
  } else if (command_id >= USE_LAUNCH_TYPE_COMMAND_START &&
             command_id < USE_LAUNCH_TYPE_COMMAND_END) {
    extensions::LaunchType launch_type = static_cast<extensions::LaunchType>(
        command_id - USE_LAUNCH_TYPE_COMMAND_START);
    // When bookmark apps are enabled, hosted apps can only toggle between
    // LAUNCH_TYPE_WINDOW and LAUNCH_TYPE_REGULAR.
    if (extensions::util::IsNewBookmarkAppsEnabled()) {
      launch_type = (controller()->GetExtensionLaunchType(profile(),
                                                          app_id()) ==
                     extensions::LAUNCH_TYPE_WINDOW)
                        ? extensions::LAUNCH_TYPE_REGULAR
                        : extensions::LAUNCH_TYPE_WINDOW;
    }
    controller()->SetExtensionLaunchType(profile(), app_id(), launch_type);
  } else if (command_id == OPTIONS) {
    controller()->ShowOptionsPage(profile(), app_id());
  } else if (command_id == UNINSTALL) {
    controller()->UninstallApp(profile(), app_id());
  } else if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
                 command_id)) {
    extension_menu_items_->ExecuteCommand(command_id, nullptr, nullptr,
                                          content::ContextMenuParams());
  } else if (command_id == MENU_NEW_WINDOW) {
    controller()->CreateNewWindow(profile(), false);
  } else if (command_id == MENU_NEW_INCOGNITO_WINDOW) {
    controller()->CreateNewWindow(profile(), true);
  } else {
    AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

}  // namespace app_list
