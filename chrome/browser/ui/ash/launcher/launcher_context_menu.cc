// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include <string>

#include "ash/public/cpp/shelf_model.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/grit/generated_resources.h"
#include "ui/display/types/display_constants.h"

// static
std::unique_ptr<LauncherContextMenu> LauncherContextMenu::Create(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    int64_t display_id) {
  DCHECK(controller);
  DCHECK(item);
  DCHECK(!item->id.IsNull());
  // Create an ArcLauncherContextMenu if the item is an ARC app.
  if (arc::IsArcItem(controller->profile(), item->id.app_id)) {
    return base::MakeUnique<ArcLauncherContextMenu>(controller, item,
                                                    display_id);
  }

  // Create an ExtensionLauncherContextMenu for other items.
  return base::MakeUnique<ExtensionLauncherContextMenu>(controller, item,
                                                        display_id);
}

LauncherContextMenu::LauncherContextMenu(ChromeLauncherController* controller,
                                         const ash::ShelfItem* item,
                                         int64_t display_id)
    : ui::SimpleMenuModel(this),
      controller_(controller),
      item_(item ? *item : ash::ShelfItem()),
      display_id_(display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);
}

LauncherContextMenu::~LauncherContextMenu() {}

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  DCHECK(command_id < MENU_ITEM_COUNT);
  return false;
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == MENU_PIN) {
    // Users cannot modify the pinned state of apps pinned by policy.
    return !item_.pinned_by_policy &&
           (item_.type == ash::TYPE_PINNED_APP || item_.type == ash::TYPE_APP);
  }

  DCHECK(command_id < MENU_ITEM_COUNT);
  return true;
}

void LauncherContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_OPEN_NEW:
      // Use a copy of the id to avoid crashes, as this menu's owner will be
      // destroyed if LaunchApp replaces the ShelfItemDelegate instance.
      controller_->LaunchApp(ash::ShelfID(item_.id), ash::LAUNCH_FROM_UNKNOWN,
                             ui::EF_NONE, display_id_);
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
      base::RecordAction(base::UserMetricsAction("CloseFromContextMenu"));
      if (TabletModeClient::Get()->tablet_mode_enabled()) {
        base::RecordAction(
            base::UserMetricsAction("Tablet_WindowCloseFromContextMenu"));
      }
      break;
    case MENU_PIN:
      if (controller_->IsAppPinned(item_.id.app_id))
        controller_->UnpinAppWithID(item_.id.app_id);
      else
        controller_->PinAppWithID(item_.id.app_id);
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

bool LauncherContextMenu::ExecuteCommonCommand(int command_id,
                                               int event_flags) {
  switch (command_id) {
    case MENU_OPEN_NEW:
    case MENU_CLOSE:
    case MENU_PIN:
      LauncherContextMenu::ExecuteCommand(command_id, event_flags);
      return true;
    default:
      return false;
  }
}
