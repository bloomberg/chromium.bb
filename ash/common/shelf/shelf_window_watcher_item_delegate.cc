// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_window_watcher_item_delegate.h"

#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"

namespace ash {

namespace {

ShelfItemType GetShelfItemType(ShelfID id) {
  ShelfModel* model = Shell::Get()->shelf_controller()->model();
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

void ShelfWindowWatcherItemDelegate::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ShelfLaunchSource source,
    const ItemSelectedCallback& callback) {
  // Move panels attached on another display to the current display.
  if (GetShelfItemType(id_) == TYPE_APP_PANEL &&
      window_->aura_window()->GetProperty(kPanelAttachedKey) &&
      wm::MoveWindowToDisplay(window_->aura_window(), display_id)) {
    window_->Activate();
    callback.Run(SHELF_ACTION_WINDOW_ACTIVATED, base::nullopt);
    return;
  }

  if (window_->IsActive()) {
    if (event && event->type() == ui::ET_KEY_RELEASED) {
      window_->Animate(::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
      callback.Run(SHELF_ACTION_NONE, base::nullopt);
      return;
    }
    window_->Minimize();
    callback.Run(SHELF_ACTION_WINDOW_MINIMIZED, base::nullopt);
    return;
  }
  window_->Activate();
  callback.Run(SHELF_ACTION_WINDOW_ACTIVATED, base::nullopt);
}

void ShelfWindowWatcherItemDelegate::ExecuteCommand(uint32_t command_id,
                                                    int32_t event_flags) {}

void ShelfWindowWatcherItemDelegate::Close() {
  window_->CloseWidget();
}

}  // namespace ash
