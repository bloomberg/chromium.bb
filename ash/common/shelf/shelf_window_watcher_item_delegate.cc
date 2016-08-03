// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_window_watcher_item_delegate.h"

#include "ash/common/shelf/shelf_item_types.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_window.h"
#include "ui/events/event.h"

namespace ash {

ShelfWindowWatcherItemDelegate::ShelfWindowWatcherItemDelegate(WmWindow* window)
    : window_(window) {}

ShelfWindowWatcherItemDelegate::~ShelfWindowWatcherItemDelegate() {}

ShelfItemDelegate::PerformedAction ShelfWindowWatcherItemDelegate::ItemSelected(
    const ui::Event& event) {
  wm::WindowState* window_state = window_->GetWindowState();
  if (window_state->IsActive()) {
    if (event.type() & ui::ET_KEY_RELEASED) {
      window_->Animate(::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
      return kNoAction;
    } else {
      window_state->Minimize();
      return kExistingWindowMinimized;
    }
  } else {
    window_state->Activate();
    return kExistingWindowActivated;
  }
}

base::string16 ShelfWindowWatcherItemDelegate::GetTitle() {
  return window_->GetShelfItemDetails()->title;
}

ShelfMenuModel* ShelfWindowWatcherItemDelegate::CreateApplicationMenu(
    int event_flags) {
  return nullptr;
}

bool ShelfWindowWatcherItemDelegate::IsDraggable() {
  return true;
}

bool ShelfWindowWatcherItemDelegate::CanPin() const {
  return true;
}

bool ShelfWindowWatcherItemDelegate::ShouldShowTooltip() {
  return true;
}

void ShelfWindowWatcherItemDelegate::Close() {
  window_->CloseWidget();
}

}  // namespace ash
