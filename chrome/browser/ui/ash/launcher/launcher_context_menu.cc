// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include <string>

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_shell.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "build/build_config.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_impl.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/desktop_shell_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/context_menu_params.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Returns true if the user can modify the |shelf|'s auto-hide behavior.
bool CanUserModifyShelfAutoHideBehavior(const Profile* profile) {
  const std::string& pref = prefs::kShelfAutoHideBehaviorLocal;
  return profile->GetPrefs()->FindPreference(pref)->IsUserModifiable();
}

}  // namespace

// static
LauncherContextMenu* LauncherContextMenu::Create(
    ChromeLauncherControllerImpl* controller,
    const ash::ShelfItem* item,
    ash::WmShelf* wm_shelf) {
  DCHECK(controller);
  DCHECK(wm_shelf);
  // Create DesktopShellLauncherContextMenu if no item is selected.
  if (!item || item->id == 0)
    return new DesktopShellLauncherContextMenu(controller, item, wm_shelf);

  // Create ArcLauncherContextMenu if the item is an Arc app.
  const std::string& app_id = controller->GetAppIDForShelfID(item->id);
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(controller->GetProfile());
  if (arc_prefs && arc_prefs->IsRegistered(app_id))
    return new ArcLauncherContextMenu(controller, item, wm_shelf);

  // Create ExtensionLauncherContextMenu for the item.
  return new ExtensionLauncherContextMenu(controller, item, wm_shelf);
}

LauncherContextMenu::LauncherContextMenu(
    ChromeLauncherControllerImpl* controller,
    const ash::ShelfItem* item,
    ash::WmShelf* wm_shelf)
    : ui::SimpleMenuModel(nullptr),
      controller_(controller),
      item_(item ? *item : ash::ShelfItem()),
      shelf_alignment_menu_(wm_shelf),
      wm_shelf_(wm_shelf) {
  set_delegate(this);
}

LauncherContextMenu::~LauncherContextMenu() {
}

bool LauncherContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return false;
}

base::string16 LauncherContextMenu::GetLabelForCommandId(int command_id) const {
  NOTREACHED();
  return base::string16();
}

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  if (command_id == MENU_AUTO_HIDE) {
    return wm_shelf_->GetAutoHideBehavior() ==
           ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  }
  DCHECK(command_id < MENU_ITEM_COUNT);
  return false;
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case MENU_PIN:
      return controller_->IsPinnable(item_.id);
    case MENU_CHANGE_WALLPAPER:
      return ash::Shell::GetInstance()
          ->user_wallpaper_delegate()
          ->CanOpenSetWallpaperPage();
    case MENU_AUTO_HIDE:
      return CanUserModifyShelfAutoHideBehavior(controller_->GetProfile());
    default:
      DCHECK(command_id < MENU_ITEM_COUNT);
      return true;
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
        ash::ShelfItemDelegate* item_delegate =
            ash::Shell::GetInstance()
                ->shelf_item_delegate_manager()
                ->GetShelfItemDelegate(item_.id);
        DCHECK(item_delegate);
        item_delegate->Close();
      } else {
        // TODO(simonhong): Use ShelfItemDelegate::Close().
        controller_->Close(item_.id);
      }
      ash::WmShell::Get()->RecordUserMetricsAction(
          ash::UMA_CLOSE_THROUGH_CONTEXT_MENU);
      break;
    case MENU_PIN:
      controller_->TogglePinned(item_.id);
      break;
    case MENU_AUTO_HIDE:
      wm_shelf_->SetAutoHideBehavior(
          wm_shelf_->GetAutoHideBehavior() ==
                  ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS
              ? ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER
              : ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
      break;
    case MENU_ALIGNMENT_MENU:
      break;
    case MENU_CHANGE_WALLPAPER:
      ash::Shell::GetInstance()->user_wallpaper_delegate()->
          OpenSetWallpaperPage();
      break;
    default:
      NOTREACHED();
  }
}

void LauncherContextMenu::AddPinMenu() {
  // Expect an item with a none zero id to add pin/unpin menu item.
  DCHECK(item_.id);
  int menu_pin_string_id;
  const std::string app_id = controller_->GetAppIDForShelfID(item_.id);
  switch (GetPinnableForAppID(app_id, controller_->GetProfile())) {
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
  // In fullscreen, the launcher is either hidden or autohidden depending
  // on thethe type of fullscreen. Do not show the auto-hide menu item while in
  // while in fullscreen because it is confusing when the preference appears
  // not to apply.
  if (!IsFullScreenMode() &&
      CanUserModifyShelfAutoHideBehavior(controller_->GetProfile())) {
    AddCheckItemWithStringId(MENU_AUTO_HIDE,
                             IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE);
  }
  if (ash::ShelfWidget::ShelfAlignmentAllowed() &&
      !ash::WmShell::Get()->GetSessionStateDelegate()->IsScreenLocked()) {
    AddSubMenuWithStringId(MENU_ALIGNMENT_MENU,
                           IDS_ASH_SHELF_CONTEXT_MENU_POSITION,
                           &shelf_alignment_menu_);
  }
  if (!controller_->GetProfile()->IsGuestSession())
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
