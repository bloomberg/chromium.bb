// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_window_watcher_item_delegate.h"

#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ash/wm/window_util.h"
#include "ui/events/event_constants.h"

namespace ash {

namespace {

ShelfItemType GetShelfItemType(ShelfID id) {
  ShelfModel* model = WmShell::Get()->shelf_controller()->model();
  ShelfItems::const_iterator item = model->ItemByID(id);
  return item == model->items().end() ? TYPE_UNDEFINED : item->type;
}

}  // namespace

ShelfWindowWatcherItemDelegate::ShelfWindowWatcherItemDelegate(ShelfID id,
                                                               WmWindow* window)
    : id_(id), window_(window) {
  DCHECK_NE(kInvalidShelfID, id_);
  DCHECK(window_);
}

ShelfWindowWatcherItemDelegate::~ShelfWindowWatcherItemDelegate() {}

ShelfAction ShelfWindowWatcherItemDelegate::ItemSelected(
    ui::EventType event_type,
    int event_flags,
    int64_t display_id,
    ShelfLaunchSource source) {
  // Move panels attached on another display to the current display.
  if (GetShelfItemType(id_) == TYPE_APP_PANEL &&
      window_->GetBoolProperty(WmWindowProperty::PANEL_ATTACHED) &&
      wm::MoveWindowToDisplay(window_->aura_window(), display_id)) {
    window_->Activate();
    return SHELF_ACTION_WINDOW_ACTIVATED;
  }

  if (window_->IsActive()) {
    if (event_type == ui::ET_KEY_RELEASED) {
      window_->Animate(::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
      return SHELF_ACTION_NONE;
    }
    window_->Minimize();
    return SHELF_ACTION_WINDOW_MINIMIZED;
  }
  window_->Activate();
  return SHELF_ACTION_WINDOW_ACTIVATED;
}

ShelfAppMenuItemList ShelfWindowWatcherItemDelegate::GetAppMenuItems(
    int event_flags) {
  // Return an empty item list to avoid showing an application menu.
  return ShelfAppMenuItemList();
}

void ShelfWindowWatcherItemDelegate::ExecuteCommand(uint32_t command_id,
                                                    int event_flags) {
  // This delegate does not support showing an application menu.
  NOTIMPLEMENTED();
}

void ShelfWindowWatcherItemDelegate::Close() {
  window_->CloseWidget();
}

}  // namespace ash
