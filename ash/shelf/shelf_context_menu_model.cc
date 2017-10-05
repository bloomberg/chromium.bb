// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_context_menu_model.h"

#include <string>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using l10n_util::GetStringUTF16;
using SubmenuList = std::vector<std::unique_ptr<ui::MenuModel>>;

namespace ash {

namespace {

// Find a menu item by command id; returns a stub item if no match was found.
const mojom::MenuItemPtr& GetItem(const MenuItemList& items, int command_id) {
  const uint32_t id = base::checked_cast<uint32_t>(command_id);
  static const mojom::MenuItemPtr item_not_found(mojom::MenuItem::New());
  for (const mojom::MenuItemPtr& item : items) {
    if (item->command_id == id)
      return item;
    if (item->type == ui::MenuModel::TYPE_SUBMENU &&
        item->submenu.has_value()) {
      const mojom::MenuItemPtr& submenu_item =
          GetItem(item->submenu.value(), command_id);
      if (submenu_item->command_id == id)
        return submenu_item;
    }
  }
  return item_not_found;
}

// A shelf context submenu model; used for shelf alignment.
class ShelfContextSubMenuModel : public ui::SimpleMenuModel {
 public:
  ShelfContextSubMenuModel(Delegate* delegate,
                           const MenuItemList& items,
                           SubmenuList* submenus)
      : ui::SimpleMenuModel(delegate) {
    ShelfContextMenuModel::AddItems(this, delegate, items, submenus);
  }
  ~ShelfContextSubMenuModel() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfContextSubMenuModel);
};

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
  // In fullscreen, the shelf is either hidden or auto-hidden, depending on
  // the type of fullscreen. Do not show the auto-hide menu item while in
  // fullscreen because it is confusing when the preference appears not to
  // apply.
  if (CanUserModifyShelfAutoHide(prefs) && !IsFullScreenMode(display_id)) {
    mojom::MenuItemPtr auto_hide(mojom::MenuItem::New());
    auto_hide->type = ui::MenuModel::TYPE_CHECK;
    auto_hide->command_id = ShelfContextMenuModel::MENU_AUTO_HIDE;
    auto_hide->label = GetStringUTF16(IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE);
    auto_hide->checked = GetShelfAutoHideBehaviorPref(prefs, display_id) ==
                         SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
    auto_hide->enabled = !is_tablet_mode;
    menu->push_back(std::move(auto_hide));
  }

  // Only allow alignment and wallpaper modifications by the owner or user.
  LoginStatus status = Shell::Get()->session_controller()->login_status();
  if (status != LoginStatus::USER && status != LoginStatus::OWNER)
    return;

  const ShelfAlignment alignment = GetShelfAlignmentPref(prefs, display_id);
  mojom::MenuItemPtr alignment_menu(mojom::MenuItem::New());
  alignment_menu->type = ui::MenuModel::TYPE_SUBMENU;
  alignment_menu->command_id = ShelfContextMenuModel::MENU_ALIGNMENT_MENU;
  alignment_menu->label = GetStringUTF16(IDS_ASH_SHELF_CONTEXT_MENU_POSITION);
  alignment_menu->submenu = MenuItemList();
  alignment_menu->enabled = !is_tablet_mode;

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

  if (Shell::Get()->wallpaper_delegate()->CanOpenSetWallpaperPage()) {
    mojom::MenuItemPtr wallpaper(mojom::MenuItem::New());
    wallpaper->command_id = ShelfContextMenuModel::MENU_CHANGE_WALLPAPER;
    wallpaper->label = GetStringUTF16(IDS_AURA_SET_DESKTOP_WALLPAPER);
    wallpaper->enabled = true;
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
  // Append some menu items that are handled locally by Ash.
  AddLocalMenuItems(&menu_items_, display_id);
  AddItems(this, this, menu_items_, &submenus_);
}

ShelfContextMenuModel::~ShelfContextMenuModel() {}

// static
void ShelfContextMenuModel::AddItems(ui::SimpleMenuModel* model,
                                     ui::SimpleMenuModel::Delegate* delegate,
                                     const MenuItemList& items,
                                     SubmenuList* submenus) {
  for (const mojom::MenuItemPtr& item : items) {
    switch (item->type) {
      case ui::MenuModel::TYPE_COMMAND:
        model->AddItem(item->command_id, item->label);
        break;
      case ui::MenuModel::TYPE_CHECK:
        model->AddCheckItem(item->command_id, item->label);
        break;
      case ui::MenuModel::TYPE_RADIO:
        model->AddRadioItem(item->command_id, item->label,
                            item->radio_group_id);
        break;
      case ui::MenuModel::TYPE_SEPARATOR:
        model->AddSeparator(ui::NORMAL_SEPARATOR);
        break;
      case ui::MenuModel::TYPE_BUTTON_ITEM:
        NOTREACHED() << "TYPE_BUTTON_ITEM is not yet supported.";
      case ui::MenuModel::TYPE_SUBMENU:
        if (item->submenu.has_value()) {
          std::unique_ptr<ui::MenuModel> submenu =
              base::MakeUnique<ShelfContextSubMenuModel>(
                  delegate, item->submenu.value(), submenus);
          model->AddSubMenu(item->command_id, item->label, submenu.get());
          submenus->push_back(std::move(submenu));
        }
        break;
    }
    if (!item->image.isNull()) {
      model->SetIcon(model->GetIndexOfCommandId(item->command_id),
                     gfx::Image(item->image));
    }
  }
}

bool ShelfContextMenuModel::IsCommandIdChecked(int command_id) const {
  return GetItem(menu_items_, command_id)->checked;
}

bool ShelfContextMenuModel::IsCommandIdEnabled(int command_id) const {
  return GetItem(menu_items_, command_id)->enabled;
}

void ShelfContextMenuModel::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(IsCommandIdEnabled(command_id));
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)  // Null during startup.
    return;

  UserMetricsRecorder* metrics = Shell::Get()->metrics();
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
      metrics->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_LEFT);
      SetShelfAlignmentPref(prefs, display_id_, SHELF_ALIGNMENT_LEFT);
      break;
    case MENU_ALIGNMENT_RIGHT:
      metrics->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_RIGHT);
      SetShelfAlignmentPref(prefs, display_id_, SHELF_ALIGNMENT_RIGHT);
      break;
    case MENU_ALIGNMENT_BOTTOM:
      metrics->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_BOTTOM);
      SetShelfAlignmentPref(prefs, display_id_, SHELF_ALIGNMENT_BOTTOM);
      break;
    case MENU_CHANGE_WALLPAPER:
      Shell::Get()->wallpaper_controller()->OpenSetWallpaperPage();
      break;
    default:
      // Have the shelf item delegate execute the context menu command.
      if (delegate_)
        delegate_->ExecuteCommand(true, command_id, event_flags, display_id_);
      break;
  }
}

}  // namespace ash
