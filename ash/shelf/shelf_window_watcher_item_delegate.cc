// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_watcher_item_delegate.h"

#include "ash/shelf/shelf_util.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/window_animations.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

ShelfWindowWatcherItemDelegate::ShelfWindowWatcherItemDelegate(
    aura::Window* window)
    : window_(window) {
}

ShelfWindowWatcherItemDelegate::~ShelfWindowWatcherItemDelegate() {
}

void ShelfWindowWatcherItemDelegate::Close() {
  views::Widget::GetWidgetForNativeWindow(window_)->Close();
}

bool ShelfWindowWatcherItemDelegate::ItemSelected(const ui::Event& event) {
  wm::WindowState* window_state = wm::GetWindowState(window_);
  if (window_state->IsActive()) {
    if (event.type() & ui::ET_KEY_RELEASED) {
      views::corewm::AnimateWindow(window_,
                                   views::corewm::WINDOW_ANIMATION_TYPE_BOUNCE);
    } else {
      window_state->Minimize();
    }
  } else {
    window_state->Activate();
  }

  return false;
}

base::string16 ShelfWindowWatcherItemDelegate::GetTitle() {
  return GetLauncherItemDetailsForWindow(window_)->title;
}

ui::MenuModel* ShelfWindowWatcherItemDelegate::CreateContextMenu(
    aura::Window* root_window) {
  // TODO(simonhong): Create ShelfItemContextMenu.
  return NULL;
}

ShelfMenuModel* ShelfWindowWatcherItemDelegate::CreateApplicationMenu(
    int event_flags) {
  return NULL;
}

bool ShelfWindowWatcherItemDelegate::IsDraggable() {
  return true;
}

bool ShelfWindowWatcherItemDelegate::ShouldShowTooltip() {
  return true;
}

}  // namespace internal
}  // namespace ash
