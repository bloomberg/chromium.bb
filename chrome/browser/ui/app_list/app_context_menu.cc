// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_context_menu.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/common/context_menu_params.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

namespace app_list {

namespace {

enum CommandId {
  LAUNCH_NEW = 100,
  TOGGLE_PIN,
  CREATE_SHORTCUTS,
  OPTIONS,
  UNINSTALL,
  DETAILS,
  MENU_NEW_WINDOW,
  MENU_NEW_INCOGNITO_WINDOW,
  // Order matters in LAUNCHER_TYPE_xxxx and must match LaunchType.
  LAUNCH_TYPE_START = 200,
  LAUNCH_TYPE_PINNED_TAB = LAUNCH_TYPE_START,
  LAUNCH_TYPE_REGULAR_TAB,
  LAUNCH_TYPE_FULLSCREEN,
  LAUNCH_TYPE_WINDOW,
  LAUNCH_TYPE_LAST,
};

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

}  // namespace

AppContextMenu::AppContextMenu(AppContextMenuDelegate* delegate,
                               Profile* profile,
                               const std::string& app_id,
                               AppListControllerDelegate* controller,
                               bool is_platform_app,
                               bool is_search_result)
    : delegate_(delegate),
      profile_(profile),
      app_id_(app_id),
      controller_(controller),
      is_platform_app_(is_platform_app),
      is_search_result_(is_search_result) {
}

AppContextMenu::~AppContextMenu() {}

ui::MenuModel* AppContextMenu::GetMenuModel() {
  if (!controller_->IsExtensionInstalled(profile_, app_id_))
    return NULL;

  if (menu_model_.get())
    return menu_model_.get();

  menu_model_.reset(new ui::SimpleMenuModel(this));

  if (app_id_ == extension_misc::kChromeAppId) {
    menu_model_->AddItemWithStringId(
        MENU_NEW_WINDOW,
        IDS_APP_LIST_NEW_WINDOW);
    if (!profile_->IsOffTheRecord()) {
      menu_model_->AddItemWithStringId(
          MENU_NEW_INCOGNITO_WINDOW,
          IDS_APP_LIST_NEW_INCOGNITO_WINDOW);
    }
  } else {
    extension_menu_items_.reset(new extensions::ContextMenuMatcher(
        profile_, this, menu_model_.get(),
        base::Bind(MenuItemHasLauncherContext)));

    if (!is_platform_app_)
      menu_model_->AddItem(LAUNCH_NEW, string16());

    int index = 0;
    extension_menu_items_->AppendExtensionItems(app_id_, string16(),
                                                &index);

    // Show Pin/Unpin option if shelf is available.
    if (controller_->GetPinnable() != AppListControllerDelegate::NO_PIN) {
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_model_->AddItemWithStringId(
          TOGGLE_PIN,
          controller_->IsAppPinned(app_id_) ?
              IDS_APP_LIST_CONTEXT_MENU_UNPIN :
              IDS_APP_LIST_CONTEXT_MENU_PIN);
    }

    if (controller_->CanDoCreateShortcutsFlow()) {
      menu_model_->AddItemWithStringId(CREATE_SHORTCUTS,
                                       IDS_NEW_TAB_APP_CREATE_SHORTCUT);
    }

    if (!is_platform_app_) {
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      // Streamlined hosted apps can only toggle between LAUNCH_WINDOW and
      // LAUNCH_REGULAR.
      if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableStreamlinedHostedApps)) {
        menu_model_->AddCheckItemWithStringId(
            LAUNCH_TYPE_REGULAR_TAB,
            IDS_APP_CONTEXT_MENU_OPEN_TAB);
      } else {
        menu_model_->AddCheckItemWithStringId(
            LAUNCH_TYPE_REGULAR_TAB,
            IDS_APP_CONTEXT_MENU_OPEN_REGULAR);
        menu_model_->AddCheckItemWithStringId(
            LAUNCH_TYPE_PINNED_TAB,
            IDS_APP_CONTEXT_MENU_OPEN_PINNED);
#if defined(OS_MACOSX)
        // Mac does not support standalone web app browser windows or maximize.
        menu_model_->AddCheckItemWithStringId(
            LAUNCH_TYPE_FULLSCREEN,
            IDS_APP_CONTEXT_MENU_OPEN_FULLSCREEN);
#else
        menu_model_->AddCheckItemWithStringId(
            LAUNCH_TYPE_WINDOW,
            IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
        // Even though the launch type is Full Screen it is more accurately
        // described as Maximized in Ash.
        menu_model_->AddCheckItemWithStringId(
            LAUNCH_TYPE_FULLSCREEN,
            IDS_APP_CONTEXT_MENU_OPEN_MAXIMIZED);
#endif
      }
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_model_->AddItemWithStringId(OPTIONS, IDS_NEW_TAB_APP_OPTIONS);
    }

    menu_model_->AddItemWithStringId(DETAILS, IDS_NEW_TAB_APP_DETAILS);
    menu_model_->AddItemWithStringId(
        UNINSTALL,
        is_platform_app_ ? IDS_APP_LIST_UNINSTALL_ITEM
                         : IDS_EXTENSIONS_UNINSTALL);
  }

  return menu_model_.get();
}

bool AppContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == TOGGLE_PIN || command_id == LAUNCH_NEW;
}

string16 AppContextMenu::GetLabelForCommandId(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return controller_->IsAppPinned(app_id_) ?
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN) :
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN);
  } else if (command_id == LAUNCH_NEW) {
#if defined(OS_MACOSX)
    // Even fullscreen windows launch in a browser tab on Mac.
    const bool launches_in_tab = true;
#else
    const bool launches_in_tab = IsCommandIdChecked(LAUNCH_TYPE_PINNED_TAB) ||
        IsCommandIdChecked(LAUNCH_TYPE_REGULAR_TAB);
#endif
    return launches_in_tab ?
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_TAB) :
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW);
  } else {
    NOTREACHED();
    return string16();
  }
}

bool AppContextMenu::IsCommandIdChecked(int command_id) const {
  if (command_id >= LAUNCH_TYPE_START && command_id < LAUNCH_TYPE_LAST) {
    return static_cast<int>(controller_->GetExtensionLaunchType(
        profile_, app_id_)) + LAUNCH_TYPE_START == command_id;
  } else if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
             command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return extension_menu_items_->IsCommandIdChecked(command_id);
  }
  return false;
}

bool AppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return controller_->GetPinnable() ==
        AppListControllerDelegate::PIN_EDITABLE;
  } else if (command_id == OPTIONS) {
    return controller_->HasOptionsPage(profile_, app_id_);
  } else if (command_id == UNINSTALL) {
    return controller_->UserMayModifySettings(profile_, app_id_);
  } else if (command_id == DETAILS) {
    return controller_->IsAppFromWebStore(profile_, app_id_);
  } else if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
             command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  } else if (command_id == MENU_NEW_WINDOW) {
    // "Normal" windows are not allowed when incognito is enforced.
    return IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
        IncognitoModePrefs::FORCED;
  } else if (command_id == MENU_NEW_INCOGNITO_WINDOW) {
    // Incognito windows are not allowed when incognito is disabled.
    return IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
        IncognitoModePrefs::DISABLED;
  }
  return true;
}

bool AppContextMenu::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* acclelrator) {
  return false;
}

void AppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == LAUNCH_NEW) {
    delegate_->ExecuteLaunchCommand(event_flags);
  } else if (command_id == TOGGLE_PIN && controller_->GetPinnable() ==
      AppListControllerDelegate::PIN_EDITABLE) {
    if (controller_->IsAppPinned(app_id_))
      controller_->UnpinApp(app_id_);
    else
      controller_->PinApp(app_id_);
  } else if (command_id == CREATE_SHORTCUTS) {
    controller_->DoCreateShortcutsFlow(profile_, app_id_);
  } else if (command_id >= LAUNCH_TYPE_START &&
             command_id < LAUNCH_TYPE_LAST) {
    extensions::ExtensionPrefs::LaunchType launch_type =
        static_cast<extensions::ExtensionPrefs::LaunchType>(
            command_id - LAUNCH_TYPE_START);
    // Streamlined hosted apps can only toggle between LAUNCH_WINDOW and
    // LAUNCH_REGULAR.
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableStreamlinedHostedApps)) {
      launch_type = controller_->GetExtensionLaunchType(profile_, app_id_) ==
                            extensions::ExtensionPrefs::LAUNCH_REGULAR
                        ? extensions::ExtensionPrefs::LAUNCH_WINDOW
                        : extensions::ExtensionPrefs::LAUNCH_REGULAR;
    }
    controller_->SetExtensionLaunchType(profile_,
                                        app_id_,
                                        launch_type);
  } else if (command_id == OPTIONS) {
    controller_->ShowOptionsPage(profile_, app_id_);
  } else if (command_id == UNINSTALL) {
    controller_->UninstallApp(profile_, app_id_);
  } else if (command_id == DETAILS) {
    controller_->ShowAppInWebStore(profile_, app_id_, is_search_result_);
  } else if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
             command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    extension_menu_items_->ExecuteCommand(command_id, NULL,
                                          content::ContextMenuParams());
  } else if (command_id == MENU_NEW_WINDOW) {
    controller_->CreateNewWindow(profile_, false);
  } else if (command_id == MENU_NEW_INCOGNITO_WINDOW) {
    controller_->CreateNewWindow(profile_, true);
  }
}

}  // namespace app_list
