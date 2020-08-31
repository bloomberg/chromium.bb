// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_context_menu_model.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "components/prefs/pref_service.h"
#include "ui/base/models/image_model.h"

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

}  // namespace

ShelfContextMenuModel::ShelfContextMenuModel(ShelfItemDelegate* delegate,
                                             int64_t display_id)
    : ui::SimpleMenuModel(this), delegate_(delegate), display_id_(display_id) {
  // Add shelf and wallpaper items if ShelfView or HomeButton are selected.
  if (!delegate)
    AddShelfAndWallpaperItems();
}

ShelfContextMenuModel::~ShelfContextMenuModel() = default;

bool ShelfContextMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id == MENU_ALIGNMENT_LEFT ||
      command_id == MENU_ALIGNMENT_BOTTOM ||
      command_id == MENU_ALIGNMENT_RIGHT) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();
    const ShelfAlignment alignment = GetShelfAlignmentPref(prefs, display_id_);
    if (command_id == MENU_ALIGNMENT_LEFT)
      return alignment == ShelfAlignment::kLeft;
    if (command_id == MENU_ALIGNMENT_BOTTOM) {
      return alignment == ShelfAlignment::kBottom ||
             alignment == ShelfAlignment::kBottomLocked;
    }
    if (command_id == MENU_ALIGNMENT_RIGHT)
      return alignment == ShelfAlignment::kRight;
  }

  return SimpleMenuModel::Delegate::IsCommandIdChecked(command_id);
}

void ShelfContextMenuModel::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(IsCommandIdEnabled(command_id));
  Shell* shell = Shell::Get();
  PrefService* prefs =
      shell->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)  // Null during startup.
    return;

  UserMetricsRecorder* metrics = shell->metrics();
  // Clamshell mode only options should not activate in tablet mode.
  const bool is_tablet_mode = shell->tablet_mode_controller()->InTabletMode();
  switch (command_id) {
    case MENU_AUTO_HIDE:
      SetShelfAutoHideBehaviorPref(
          prefs, display_id_,
          GetShelfAutoHideBehaviorPref(prefs, display_id_) ==
                  ShelfAutoHideBehavior::kAlways
              ? ShelfAutoHideBehavior::kNever
              : ShelfAutoHideBehavior::kAlways);
      break;
    case MENU_ALIGNMENT_LEFT:
      DCHECK(!is_tablet_mode);
      metrics->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_LEFT);
      SetShelfAlignmentPref(prefs, display_id_, ShelfAlignment::kLeft);
      break;
    case MENU_ALIGNMENT_RIGHT:
      DCHECK(!is_tablet_mode);
      metrics->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_RIGHT);
      SetShelfAlignmentPref(prefs, display_id_, ShelfAlignment::kRight);
      break;
    case MENU_ALIGNMENT_BOTTOM:
      DCHECK(!is_tablet_mode);
      metrics->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_BOTTOM);
      SetShelfAlignmentPref(prefs, display_id_, ShelfAlignment::kBottom);
      break;
    case MENU_CHANGE_WALLPAPER:
      shell->wallpaper_controller()->OpenWallpaperPickerIfAllowed();
      break;
    default:
      if (delegate_) {
        if (IsCommandIdAnAppLaunch(command_id)) {
          shell->app_list_controller()->RecordShelfAppLaunched();
        }

        delegate_->ExecuteCommand(true, command_id, event_flags, display_id_);
      }
      break;
  }
}

void ShelfContextMenuModel::AddShelfAndWallpaperItems() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)  // Null during startup.
    return;

  // In fullscreen, the shelf is either hidden or auto-hidden, depending on the
  // type of fullscreen. Do not show the auto-hide menu item while in fullscreen
  // because it is confusing when the preference appears not to apply.
  if (CanUserModifyShelfAutoHide(prefs) && !IsFullScreenMode(display_id_)) {
    const bool is_autohide_set =
        GetShelfAutoHideBehaviorPref(prefs, display_id_) ==
        ShelfAutoHideBehavior::kAlways;
    auto string_id = is_autohide_set
                         ? IDS_ASH_SHELF_CONTEXT_MENU_ALWAYS_SHOW_SHELF
                         : IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE;
    AddItemWithStringIdAndIcon(
        MENU_AUTO_HIDE, string_id,
        ui::ImageModel::FromVectorIcon(is_autohide_set ? kAlwaysShowShelfIcon
                                                       : kAutoHideIcon));
  }

  // Only allow shelf alignment modifications by the owner or user. In tablet
  // mode, the shelf alignment option is not shown.
  LoginStatus status = Shell::Get()->session_controller()->login_status();
  if (status == LoginStatus::USER &&
      !Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      prefs->FindPreference(prefs::kShelfAlignmentLocal)->IsUserModifiable()) {
    alignment_submenu_ = std::make_unique<ui::SimpleMenuModel>(this);

    constexpr int group = 0;
    alignment_submenu_->AddRadioItemWithStringId(
        MENU_ALIGNMENT_LEFT, IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_LEFT, group);
    alignment_submenu_->AddRadioItemWithStringId(
        MENU_ALIGNMENT_BOTTOM, IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_BOTTOM, group);
    alignment_submenu_->AddRadioItemWithStringId(
        MENU_ALIGNMENT_RIGHT, IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_RIGHT, group);

    AddSubMenuWithStringIdAndIcon(
        MENU_ALIGNMENT_MENU, IDS_ASH_SHELF_CONTEXT_MENU_POSITION,
        alignment_submenu_.get(),
        ui::ImageModel::FromVectorIcon(kShelfPositionIcon));
  }

  if (Shell::Get()->wallpaper_controller()->CanOpenWallpaperPicker()) {
    AddItemWithStringIdAndIcon(MENU_CHANGE_WALLPAPER,
                               IDS_AURA_SET_DESKTOP_WALLPAPER,
                               ui::ImageModel::FromVectorIcon(kWallpaperIcon));
  }
}

}  // namespace ash
