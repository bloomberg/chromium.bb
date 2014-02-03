// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_watcher_shelf_item_delegate.h"

#include "ash/shell/window_watcher.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace shell {

WindowWatcherShelfItemDelegate::WindowWatcherShelfItemDelegate(
    ShelfID id,
    WindowWatcher* watcher)
    : id_(id), watcher_(watcher) {
  DCHECK_GT(id_, 0);
  DCHECK(watcher_);
}

WindowWatcherShelfItemDelegate::~WindowWatcherShelfItemDelegate() {
}

bool WindowWatcherShelfItemDelegate::ItemSelected(const ui::Event& event) {
  aura::Window* window = watcher_->GetWindowByID(id_);
  if (window->type() == ui::wm::WINDOW_TYPE_PANEL)
    wm::MoveWindowToEventRoot(window, event);
  window->Show();
  wm::ActivateWindow(window);
  return false;
}

base::string16 WindowWatcherShelfItemDelegate::GetTitle() {
  return watcher_->GetWindowByID(id_)->title();
}

ui::MenuModel* WindowWatcherShelfItemDelegate::CreateContextMenu(
    aura::Window* root_window) {
  return NULL;
}

ShelfMenuModel* WindowWatcherShelfItemDelegate::CreateApplicationMenu(
    int event_flags) {
  return NULL;
}

bool WindowWatcherShelfItemDelegate::IsDraggable() {
  return true;
}

bool WindowWatcherShelfItemDelegate::ShouldShowTooltip() {
  return true;
}

void WindowWatcherShelfItemDelegate::Close() {
}

}  // namespace shell
}  // namespace ash
