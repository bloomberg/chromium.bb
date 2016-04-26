// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_watcher_item_delegate.h"

#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

ShelfWindowWatcherItemDelegate::ShelfWindowWatcherItemDelegate(
    aura::Window* window)
    : window_(window) {}

ShelfWindowWatcherItemDelegate::~ShelfWindowWatcherItemDelegate() {}

void ShelfWindowWatcherItemDelegate::Close() {
  views::Widget::GetWidgetForNativeWindow(window_)->Close();
}

ShelfItemDelegate::PerformedAction ShelfWindowWatcherItemDelegate::ItemSelected(
    const ui::Event& event) {
  wm::WindowState* window_state = wm::GetWindowState(window_);
  if (window_state->IsActive()) {
    if (event.type() & ui::ET_KEY_RELEASED) {
      ::wm::AnimateWindow(window_, ::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
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
  return GetShelfItemDetailsForWindow(window_)->title;
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

}  // namespace ash
