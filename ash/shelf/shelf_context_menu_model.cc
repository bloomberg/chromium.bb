// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_context_menu_model.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/menu_utils.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_config.h"

using l10n_util::GetStringUTF16;
using SubmenuList = std::vector<std::unique_ptr<ui::MenuModel>>;

namespace ash {

namespace {

// Returns true if the user can modify the shelf's auto-hide behavior pref.
bool CanUserModifyShelfAutoHide(PrefService* prefs) {
  return prefs && prefs->FindPreference(prefs::kShelfAutoHideBehaviorLocal)
                      ->IsUserModifiable();
}

// Returns true if the display is showing a fullscreen window.
// NOTE: This duplicates the functionality of Chrome's IsFullScreenMode.
bool IsFullScreenMode(int64_t display_id) {
  auto* controller = Shell::GetRootWindowControllerWithDisplayId(display_id);
  return controller && controller->GetWindowForFullscreenMode();
}

// Add the context menu items to change shelf auto-hide and alignment settings
// and to change the wallpaper for the display with the given |display_id|.
void AddLocalMenuItems(MenuItemList* menu, int64_t display_id) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)  // Null during startup.
    return;

  const bool is_tablet_mode = Shell::Get()
                                  ->tablet_mode_controller()
                                  ->IsTabletModeWindowManagerEnabled();

  const views::MenuConfig& menu_config = views::MenuConfig::instance();

  // In fullscreen, the shelf is either hidden or auto-hidden, depending on
  // the type of fullscreen. Do not show the auto-hide menu item while in
  // fullscreen because it is confusing when the preference appears not to
  // apply.
  if (CanUserModifyShelfAutoHide(prefs) && !IsFullScreenMode(display_id)) {
    mojom::MenuItemPtr auto_hide(mojom::MenuItem::New());
    const bool is_autohide_set =
        GetShelfAutoHideBehaviorPref(prefs, display_id) ==
        SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
      auto_hide->type = ui::MenuModel::TYPE_COMMAND;
      auto_hide->image = gfx::CreateVectorIcon(
          is_autohide_set ? kAlwaysShowShelfIcon : kAutoHideIcon,
          menu_config.touchable_icon_size, menu_config.touchable_icon_color);
      auto_hide->label = GetStringUTF16(
          is_autohide_set ? IDS_ASH_SHELF_CONTEXT_MENU_ALWAYS_SHOW_SHELF
                          : IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE);

    auto_hide->command_id = ShelfContextMenuModel::MENU_AUTO_HIDE;
    auto_hide->enabled = true;
    menu->push_back(std::move(auto_hide));
  }

  // Only allow shelf alignment modifications by the owner or user. In tablet
  // mode, the shelf alignment option is not shown.
  LoginStatus status = Shell::Get()->session_controller()->login_status();
  if ((status == LoginStatus::USER || status == LoginStatus::OWNER) &&
      !is_tablet_mode) {
    const ShelfAlignment alignment = GetShelfAlignmentPref(prefs, display_id);
    mojom::MenuItemPtr alignment_menu(mojom::MenuItem::New());
    alignment_menu->type = ui::MenuModel::TYPE_SUBMENU;
    alignment_menu->command_id = ShelfContextMenuModel::MENU_ALIGNMENT_MENU;
    alignment_menu->label = GetStringUTF16(IDS_ASH_SHELF_CONTEXT_MENU_POSITION);
    alignment_menu->submenu = MenuItemList();
    alignment_menu->enabled = !is_tablet_mode;
    alignment_menu->image = gfx::CreateVectorIcon(
        kShelfPositionIcon, menu_config.touchable_icon_size,
        menu_config.touchable_icon_color);

    mojom::MenuItemPtr left(mojom::MenuItem::New());
    left->type = ui::MenuModel::TYPE_RADIO;
    left->command_id = ShelfContextMenuModel::MENU_ALIGNMENT_LEFT;
    left->label = GetStringUTF16(IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_LEFT);
    left->checked = alignment == SHELF_ALIGNMENT_LEFT;
    left->enabled = true;
    alignment_menu->submenu->push_back(std::move(left));

    mojom::MenuItemPtr bottom(mojom::MenuItem::New());
    bottom->type = ui::MenuModel::TYPE_RADIO;
    bottom->command_id = ShelfContextMenuModel::MENU_ALIGNMENT_BOTTOM;
    bottom->label = GetStringUTF16(IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_BOTTOM);
    bottom->checked = alignment == SHELF_ALIGNMENT_BOTTOM ||
                      alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED;
    bottom->enabled = true;
    alignment_menu->submenu->push_back(std::move(bottom));

    mojom::MenuItemPtr right(mojom::MenuItem::New());
    right->type = ui::MenuModel::TYPE_RADIO;
    right->command_id = ShelfContextMenuModel::MENU_ALIGNMENT_RIGHT;
    right->label = GetStringUTF16(IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_RIGHT);
    right->checked = alignment == SHELF_ALIGNMENT_RIGHT;
    right->enabled = true;
    alignment_menu->submenu->push_back(std::move(right));

    menu->push_back(std::move(alignment_menu));
  }

  if (Shell::Get()->wallpaper_controller()->CanOpenWallpaperPicker()) {
    mojom::MenuItemPtr wallpaper(mojom::MenuItem::New());
    wallpaper->command_id = ShelfContextMenuModel::MENU_CHANGE_WALLPAPER;
    wallpaper->label = GetStringUTF16(IDS_AURA_SET_DESKTOP_WALLPAPER);
    wallpaper->enabled = true;
    wallpaper->image =
        gfx::CreateVectorIcon(kWallpaperIcon, menu_config.touchable_icon_size,
                              menu_config.touchable_icon_color);
    menu->push_back(std::move(wallpaper));
  }
}

}  // namespace

ShelfContextMenuModel::ShelfContextMenuModel(MenuItemList menu_items,
                                             ShelfItemDelegate* delegate,
                                             int64_t display_id)
    : ui::SimpleMenuModel(this),
      menu_items_(std::move(menu_items)),
      delegate_(delegate),
      display_id_(display_id) {
  // Append shelf settings and wallpaper items to the menu if ShelfView or
  // AppListButton are selected.
  if (!delegate || delegate_->app_id() == kAppListId)
    AddLocalMenuItems(&menu_items_, display_id);
  menu_utils::PopulateMenuFromMojoMenuItems(this, this, menu_items_,
                                            &submenus_);
}

ShelfContextMenuModel::~ShelfContextMenuModel() = default;

bool ShelfContextMenuModel::IsCommandIdChecked(int command_id) const {
  return menu_utils::GetMenuItemByCommandId(menu_items_, command_id)->checked;
}

bool ShelfContextMenuModel::IsCommandIdEnabled(int command_id) const {
  // NOTIFICATION_CONTAINER is always enabled. It is added to this model by
  // NotificationMenuController, but it is not added to |menu_items_|, so check
  // for it first.
  if (command_id == ash::NOTIFICATION_CONTAINER)
    return true;

  return menu_utils::GetMenuItemByCommandId(menu_items_, command_id)->enabled;
}

void ShelfContextMenuModel::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(IsCommandIdEnabled(command_id));
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)  // Null during startup.
    return;

  UserMetricsRecorder* metrics = Shell::Get()->metrics();
  // Clamshell mode only options should not activate in tablet mode.
  const bool is_tablet_mode = Shell::Get()
                                  ->tablet_mode_controller()
                                  ->IsTabletModeWindowManagerEnabled();
  switch (command_id) {
    case MENU_AUTO_HIDE:
      SetShelfAutoHideBehaviorPref(
          prefs, display_id_,
          GetShelfAutoHideBehaviorPref(prefs, display_id_) ==
                  SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS
              ? SHELF_AUTO_HIDE_BEHAVIOR_NEVER
              : SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
      break;
    case MENU_ALIGNMENT_LEFT:
      DCHECK(!is_tablet_mode);
      metrics->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_LEFT);
      SetShelfAlignmentPref(prefs, display_id_, SHELF_ALIGNMENT_LEFT);
      break;
    case MENU_ALIGNMENT_RIGHT:
      DCHECK(!is_tablet_mode);
      metrics->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_RIGHT);
      SetShelfAlignmentPref(prefs, display_id_, SHELF_ALIGNMENT_RIGHT);
      break;
    case MENU_ALIGNMENT_BOTTOM:
      DCHECK(!is_tablet_mode);
      metrics->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_BOTTOM);
      SetShelfAlignmentPref(prefs, display_id_, SHELF_ALIGNMENT_BOTTOM);
      break;
    case MENU_CHANGE_WALLPAPER:
      Shell::Get()->wallpaper_controller()->OpenWallpaperPickerIfAllowed();
      break;
    default:
      // Have the shelf item delegate execute the context menu command.
      if (delegate_)
        delegate_->ExecuteCommand(true, command_id, event_flags, display_id_);
      break;
  }
}

}  // namespace ash
