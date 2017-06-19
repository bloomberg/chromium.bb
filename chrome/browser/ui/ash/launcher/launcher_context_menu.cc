// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include <string>

#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wallpaper/wallpaper_delegate.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/desktop_shell_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/common/context_menu_params.h"

namespace {

// Returns true if the user can modify the |shelf|'s auto-hide behavior.
bool CanUserModifyShelfAutoHideBehavior(const Profile* profile) {
  const std::string& pref = prefs::kShelfAutoHideBehaviorLocal;
  return profile->GetPrefs()->FindPreference(pref)->IsUserModifiable();
}

}  // namespace

// static
LauncherContextMenu* LauncherContextMenu::Create(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    ash::Shelf* shelf) {
  DCHECK(controller);
  DCHECK(shelf);
  // Create DesktopShellLauncherContextMenu if no item is selected.
  if (!item || item->id.IsNull())
    return new DesktopShellLauncherContextMenu(controller, item, shelf);

  // Create ArcLauncherContextMenu if the item is an ARC app.
  if (arc::IsArcItem(controller->profile(), item->id.app_id))
    return new ArcLauncherContextMenu(controller, item, shelf);

  // Create ExtensionLauncherContextMenu for the item.
  return new ExtensionLauncherContextMenu(controller, item, shelf);
}

LauncherContextMenu::LauncherContextMenu(ChromeLauncherController* controller,
                                         const ash::ShelfItem* item,
                                         ash::Shelf* shelf)
    : ui::SimpleMenuModel(nullptr),
      controller_(controller),
      item_(item ? *item : ash::ShelfItem()),
      shelf_alignment_menu_(shelf),
      shelf_(shelf) {
  set_delegate(this);
}

LauncherContextMenu::~LauncherContextMenu() {}

bool LauncherContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return false;
}

base::string16 LauncherContextMenu::GetLabelForCommandId(int command_id) const {
  NOTREACHED();
  return base::string16();
}

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  if (command_id == MENU_AUTO_HIDE) {
    return shelf_->auto_hide_behavior() == ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  }
  DCHECK(command_id < MENU_ITEM_COUNT);
  return false;
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case MENU_PIN:
      // Users cannot modify the pinned state of apps pinned by policy.
      return !item_.pinned_by_policy && (item_.type == ash::TYPE_PINNED_APP ||
                                         item_.type == ash::TYPE_APP);
    case MENU_CHANGE_WALLPAPER:
      return ash::Shell::Get()->wallpaper_delegate()->CanOpenSetWallpaperPage();
    case MENU_AUTO_HIDE:
      return CanUserModifyShelfAutoHideBehavior(controller_->profile());
    default:
      DCHECK(command_id < MENU_ITEM_COUNT);
      return true;
  }
}

void LauncherContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_OPEN_NEW:
      controller_->LaunchApp(item_.id, ash::LAUNCH_FROM_UNKNOWN, ui::EF_NONE,
                             display::Screen::GetScreen()
                                 ->GetDisplayNearestWindow(
                                     shelf_->shelf_widget()->GetNativeWindow())
                                 .id());
      break;
    case MENU_CLOSE:
      if (item_.type == ash::TYPE_DIALOG) {
        ash::ShelfItemDelegate* item_delegate =
            controller_->shelf_model()->GetShelfItemDelegate(item_.id);
        DCHECK(item_delegate);
        item_delegate->Close();
      } else {
        // TODO(simonhong): Use ShelfItemDelegate::Close().
        controller_->Close(item_.id);
      }
      ash::ShellPort::Get()->RecordUserMetricsAction(
          ash::UMA_CLOSE_THROUGH_CONTEXT_MENU);
      if (ash::Shell::Get()
              ->maximize_mode_controller()
              ->IsMaximizeModeWindowManagerEnabled()) {
        ash::ShellPort::Get()->RecordUserMetricsAction(
            ash::UMA_TABLET_WINDOW_CLOSE_THROUGH_CONTXT_MENU);
      }
      break;
    case MENU_PIN:
      if (controller_->IsAppPinned(item_.id.app_id))
        controller_->UnpinAppWithID(item_.id.app_id);
      else
        controller_->PinAppWithID(item_.id.app_id);
      break;
    case MENU_AUTO_HIDE:
      shelf_->SetAutoHideBehavior(shelf_->auto_hide_behavior() ==
                                          ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS
                                      ? ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER
                                      : ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
      break;
    case MENU_ALIGNMENT_MENU:
      break;
    case MENU_CHANGE_WALLPAPER:
      chromeos::WallpaperManager::Get()->Open();
      break;
    default:
      NOTREACHED();
  }
}

void LauncherContextMenu::AddPinMenu() {
  // Expect a valid ShelfID to add pin/unpin menu item.
  DCHECK(!item_.id.IsNull());
  int menu_pin_string_id;
  switch (GetPinnableForAppID(item_.id.app_id, controller_->profile())) {
    case AppListControllerDelegate::PIN_EDITABLE:
      menu_pin_string_id = controller_->IsPinned(item_.id)
                               ? IDS_LAUNCHER_CONTEXT_MENU_UNPIN
                               : IDS_LAUNCHER_CONTEXT_MENU_PIN;
      break;
    case AppListControllerDelegate::PIN_FIXED:
      menu_pin_string_id = IDS_LAUNCHER_CONTEXT_MENU_PIN_ENFORCED_BY_POLICY;
      break;
    case AppListControllerDelegate::NO_PIN:
      return;
    default:
      NOTREACHED();
      return;
  }
  AddItemWithStringId(MENU_PIN, menu_pin_string_id);
}

void LauncherContextMenu::AddShelfOptionsMenu() {
  // In fullscreen, the launcher is either hidden or auto hidden depending
  // on the type of fullscreen. Do not show the auto-hide menu item while in
  // fullscreen per display because it is confusing when the preference appears
  // not to apply.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          shelf_->GetWindow()->GetRootWindow());
  if (!IsFullScreenMode(display.id()) &&
      CanUserModifyShelfAutoHideBehavior(controller_->profile())) {
    AddCheckItemWithStringId(MENU_AUTO_HIDE,
                             IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE);
  }
  if (ash::Shelf::CanChangeShelfAlignment() &&
      !session_manager::SessionManager::Get()->IsScreenLocked()) {
    AddSubMenuWithStringId(MENU_ALIGNMENT_MENU,
                           IDS_ASH_SHELF_CONTEXT_MENU_POSITION,
                           &shelf_alignment_menu_);
  }
  if (!controller_->profile()->IsGuestSession())
    AddItemWithStringId(MENU_CHANGE_WALLPAPER, IDS_AURA_SET_DESKTOP_WALLPAPER);
}

bool LauncherContextMenu::ExecuteCommonCommand(int command_id,
                                               int event_flags) {
  switch (command_id) {
    case MENU_OPEN_NEW:
    case MENU_CLOSE:
    case MENU_PIN:
    case MENU_AUTO_HIDE:
    case MENU_ALIGNMENT_MENU:
    case MENU_CHANGE_WALLPAPER:
      LauncherContextMenu::ExecuteCommand(command_id, event_flags);
      return true;
    default:
      return false;
  }
}
