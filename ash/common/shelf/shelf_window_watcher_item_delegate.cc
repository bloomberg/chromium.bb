// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_window_watcher_item_delegate.h"

#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ui/events/event.h"

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

ShelfItemDelegate::PerformedAction ShelfWindowWatcherItemDelegate::ItemSelected(
    const ui::Event& event) {
  // Move panels attached on another display to the current display.
  if (GetShelfItemType(id_) == TYPE_APP_PANEL &&
      window_->GetWindowState()->panel_attached() &&
      window_->MoveToEventRoot(event)) {
    window_->Activate();
    return kExistingWindowActivated;
  }

  if (window_->IsActive()) {
    if (event.type() & ui::ET_KEY_RELEASED) {
      window_->Animate(::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
      return kNoAction;
    }
    window_->Minimize();
    return kExistingWindowMinimized;
  }
  window_->Activate();
  return kExistingWindowActivated;
}

base::string16 ShelfWindowWatcherItemDelegate::GetTitle() {
  return window_->GetTitle();
}

ShelfMenuModel* ShelfWindowWatcherItemDelegate::CreateApplicationMenu(
    int event_flags) {
  return nullptr;
}

bool ShelfWindowWatcherItemDelegate::IsDraggable() {
  return true;
}

bool ShelfWindowWatcherItemDelegate::CanPin() const {
  return GetShelfItemType(id_) != TYPE_APP_PANEL;
}

bool ShelfWindowWatcherItemDelegate::ShouldShowTooltip() {
  // Do not show tooltips for visible attached app panel windows.
  return GetShelfItemType(id_) != TYPE_APP_PANEL || !window_->IsVisible() ||
         !window_->GetWindowState()->panel_attached();
}

void ShelfWindowWatcherItemDelegate::Close() {
  window_->CloseWidget();
}

}  // namespace ash
