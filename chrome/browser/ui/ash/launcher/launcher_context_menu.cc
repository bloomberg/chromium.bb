// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include <string>

#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/context_menu_params.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

}  // namespace

LauncherContextMenu::LauncherContextMenu(ChromeLauncherController* controller,
                                         const ash::ShelfItem* item,
                                         aura::Window* root)
    : ui::SimpleMenuModel(NULL),
      controller_(controller),
      item_(*item),
      shelf_alignment_menu_(root),
      root_window_(root),
      item_delegate_(NULL) {
  DCHECK(item);
  DCHECK(root_window_);
  Init();
}

LauncherContextMenu::LauncherContextMenu(ChromeLauncherController* controller,
                                         ash::ShelfItemDelegate* item_delegate,
                                         ash::ShelfItem* item,
                                         aura::Window* root)
    : ui::SimpleMenuModel(NULL),
      controller_(controller),
      item_(*item),
      shelf_alignment_menu_(root),
      root_window_(root),
      item_delegate_(item_delegate) {
  DCHECK(item);
  DCHECK(root_window_);
  Init();
}

LauncherContextMenu::LauncherContextMenu(ChromeLauncherController* controller,
                                         aura::Window* root)
    : ui::SimpleMenuModel(NULL),
      controller_(controller),
      item_(ash::ShelfItem()),
      shelf_alignment_menu_(root),
      extension_items_(new extensions::ContextMenuMatcher(
          controller->profile(), this, this,
          base::Bind(MenuItemHasLauncherContext))),
      root_window_(root),
      item_delegate_(NULL) {
  DCHECK(root_window_);
  Init();
}

void LauncherContextMenu::Init() {
  extension_items_.reset(new extensions::ContextMenuMatcher(
      controller_->profile(), this, this,
      base::Bind(MenuItemHasLauncherContext)));
  set_delegate(this);

  if (is_valid_item()) {
    if (item_.type == ash::TYPE_APP_SHORTCUT ||
        item_.type == ash::TYPE_WINDOWED_APP) {
      // V1 apps can be started from the menu - but V2 apps should not.
      if  (!controller_->IsPlatformApp(item_.id)) {
        AddItem(MENU_OPEN_NEW, base::string16());
        AddSeparator(ui::NORMAL_SEPARATOR);
      }
      const std::string app_id = controller_->GetAppIDForShelfID(item_.id);
      int menu_pin_string_id;
      if (!controller_->CanPin(app_id))
        menu_pin_string_id = IDS_LAUNCHER_CONTEXT_MENU_PIN_ENFORCED_BY_POLICY;
      else if (controller_->IsPinned(item_.id))
        menu_pin_string_id = IDS_LAUNCHER_CONTEXT_MENU_UNPIN;
      else
        menu_pin_string_id = IDS_LAUNCHER_CONTEXT_MENU_PIN;
      AddItem(MENU_PIN, l10n_util::GetStringUTF16(menu_pin_string_id));
      if (controller_->IsOpen(item_.id)) {
        AddItem(MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
      }
      if (!controller_->IsPlatformApp(item_.id) &&
          item_.type != ash::TYPE_WINDOWED_APP) {
        AddSeparator(ui::NORMAL_SEPARATOR);
        if (extensions::util::IsNewBookmarkAppsEnabled()) {
          // With bookmark apps enabled, hosted apps launch in a window by
          // default. This menu item is re-interpreted as a single, toggle-able
          // option to launch the hosted app as a tab.
          AddCheckItemWithStringId(LAUNCH_TYPE_WINDOW,
                                   IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
        } else {
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
      }
    } else if (item_.type == ash::TYPE_BROWSER_SHORTCUT) {
      AddItem(MENU_NEW_WINDOW,
              l10n_util::GetStringUTF16(IDS_APP_LIST_NEW_WINDOW));
      if (!controller_->IsLoggedInAsGuest()) {
        AddItem(MENU_NEW_INCOGNITO_WINDOW,
                l10n_util::GetStringUTF16(IDS_APP_LIST_NEW_INCOGNITO_WINDOW));
      }
    } else if (item_.type == ash::TYPE_DIALOG) {
      AddItem(MENU_CLOSE,
              l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
    } else {
      if (item_.type == ash::TYPE_PLATFORM_APP) {
        AddItem(
            MENU_PIN,
            l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_PIN));
      }
      bool show_close_button = controller_->IsOpen(item_.id);
#if defined(OS_CHROMEOS)
      if (extension_misc::IsImeMenuExtensionId(
              controller_->GetAppIDForShelfID(item_.id))) {
        show_close_button = false;
      }
#endif
      if (show_close_button) {
        AddItem(MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
      }
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    if (item_.type == ash::TYPE_APP_SHORTCUT ||
        item_.type == ash::TYPE_WINDOWED_APP ||
        item_.type == ash::TYPE_PLATFORM_APP) {
      const extensions::MenuItem::ExtensionKey app_key(
          controller_->GetAppIDForShelfID(item_.id));
      if (!app_key.empty()) {
        int index = 0;
        extension_items_->AppendExtensionItems(app_key,
                                               base::string16(),
                                               &index,
                                               false);  // is_action_menu
        AddSeparator(ui::NORMAL_SEPARATOR);
      }
    }
  }
  // In fullscreen, the launcher is either hidden or autohidden depending on
  // the type of fullscreen. Do not show the auto-hide menu item while in
  // fullscreen because it is confusing when the preference appears not to
  // apply.
  if (!IsFullScreenMode() &&
      controller_->CanUserModifyShelfAutoHideBehavior(root_window_)) {
    AddCheckItemWithStringId(MENU_AUTO_HIDE,
                             IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE);
  }
  if (ash::ShelfWidget::ShelfAlignmentAllowed() &&
      !ash::Shell::GetInstance()->session_state_delegate()->IsScreenLocked()) {
    AddSubMenuWithStringId(MENU_ALIGNMENT_MENU,
                           IDS_ASH_SHELF_CONTEXT_MENU_POSITION,
                           &shelf_alignment_menu_);
  }
#if defined(OS_CHROMEOS)
  if (!controller_->IsLoggedInAsGuest()) {
    AddItem(MENU_CHANGE_WALLPAPER,
         l10n_util::GetStringUTF16(IDS_AURA_SET_DESKTOP_WALLPAPER));
  }
#endif
}

LauncherContextMenu::~LauncherContextMenu() {
}

bool LauncherContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == MENU_OPEN_NEW;
}

base::string16 LauncherContextMenu::GetLabelForCommandId(int command_id) const {
  if (command_id == MENU_OPEN_NEW) {
    if (item_.type == ash::TYPE_PLATFORM_APP) {
      return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW);
    }
    switch (controller_->GetLaunchType(item_.id)) {
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

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case LAUNCH_TYPE_PINNED_TAB:
      return controller_->GetLaunchType(item_.id) ==
          extensions::LAUNCH_TYPE_PINNED;
    case LAUNCH_TYPE_REGULAR_TAB:
      return controller_->GetLaunchType(item_.id) ==
          extensions::LAUNCH_TYPE_REGULAR;
    case LAUNCH_TYPE_WINDOW:
      return controller_->GetLaunchType(item_.id) ==
          extensions::LAUNCH_TYPE_WINDOW;
    case LAUNCH_TYPE_FULLSCREEN:
      return controller_->GetLaunchType(item_.id) ==
          extensions::LAUNCH_TYPE_FULLSCREEN;
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
      return controller_->IsPinnable(item_.id);
#if defined(OS_CHROMEOS)
    case MENU_CHANGE_WALLPAPER:
      return ash::Shell::GetInstance()->user_wallpaper_delegate()->
          CanOpenSetWallpaperPage();
#endif
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

void LauncherContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_OPEN_NEW:
      controller_->Launch(item_.id, ui::EF_NONE);
      break;
    case MENU_CLOSE:
      if (item_.type == ash::TYPE_DIALOG) {
        DCHECK(item_delegate_);
        item_delegate_->Close();
      } else {
        // TODO(simonhong): Use ShelfItemDelegate::Close().
        controller_->Close(item_.id);
      }
      ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          ash::UMA_CLOSE_THROUGH_CONTEXT_MENU);
      break;
    case MENU_PIN:
      controller_->TogglePinned(item_.id);
      break;
    case LAUNCH_TYPE_PINNED_TAB:
      controller_->SetLaunchType(item_.id, extensions::LAUNCH_TYPE_PINNED);
      break;
    case LAUNCH_TYPE_REGULAR_TAB:
      controller_->SetLaunchType(item_.id, extensions::LAUNCH_TYPE_REGULAR);
      break;
    case LAUNCH_TYPE_WINDOW: {
      extensions::LaunchType launch_type = extensions::LAUNCH_TYPE_WINDOW;
      // With bookmark apps enabled, hosted apps can only toggle between
      // LAUNCH_WINDOW and LAUNCH_REGULAR.
      if (extensions::util::IsNewBookmarkAppsEnabled()) {
        launch_type = controller_->GetLaunchType(item_.id) ==
                              extensions::LAUNCH_TYPE_WINDOW
                          ? extensions::LAUNCH_TYPE_REGULAR
                          : extensions::LAUNCH_TYPE_WINDOW;
      }
      controller_->SetLaunchType(item_.id, launch_type);
      break;
    }
    case LAUNCH_TYPE_FULLSCREEN:
      controller_->SetLaunchType(item_.id, extensions::LAUNCH_TYPE_FULLSCREEN);
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
#if defined(OS_CHROMEOS)
    case MENU_CHANGE_WALLPAPER:
      ash::Shell::GetInstance()->user_wallpaper_delegate()->
          OpenSetWallpaperPage();
      break;
#endif
    default:
      extension_items_->ExecuteCommand(command_id, NULL,
                                       content::ContextMenuParams());
  }
}
