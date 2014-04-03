// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_watcher_item_delegate.h"

#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

ShelfWindowWatcherItemDelegate::ShelfWindowWatcherItemDelegate(
    aura::Window* window, ShelfModel* model)
    : window_(window),
      model_(model) {
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
      ::wm::AnimateWindow(window_, ::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    } else {
      window_state->Minimize();
    }
  } else {
    window_state->Activate();
  }

  return false;
}

base::string16 ShelfWindowWatcherItemDelegate::GetTitle() {
  return GetShelfItemDetailsForWindow(window_)->title;
}

ui::MenuModel* ShelfWindowWatcherItemDelegate::CreateContextMenu(
    aura::Window* root_window) {
  ash::ShelfItem item = *(model_->ItemByID(GetShelfIDForWindow(window_)));
  return Shell::GetInstance()->delegate()->CreateContextMenu(root_window,
                                                             this,
                                                             &item);
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

}  // namespace ash
