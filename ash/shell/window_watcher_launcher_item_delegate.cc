// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_watcher_launcher_item_delegate.h"

#include "ash/shell/window_watcher.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace shell {

WindowWatcherLauncherItemDelegate::WindowWatcherLauncherItemDelegate(
    ash::LauncherID id,
    ash::shell::WindowWatcher* watcher)
    : id_(id),
      watcher_(watcher) {
  DCHECK_GT(id_, 0);
  DCHECK(watcher_);
}

WindowWatcherLauncherItemDelegate::~WindowWatcherLauncherItemDelegate() {
}

bool WindowWatcherLauncherItemDelegate::ItemSelected(const ui::Event& event) {
  aura::Window* window = watcher_->GetWindowByID(id_);
  if (window->type() == aura::client::WINDOW_TYPE_PANEL)
    ash::wm::MoveWindowToEventRoot(window, event);
  window->Show();
  ash::wm::ActivateWindow(window);
  return false;
}

base::string16 WindowWatcherLauncherItemDelegate::GetTitle() {
  return watcher_->GetWindowByID(id_)->title();
}

ui::MenuModel* WindowWatcherLauncherItemDelegate::CreateContextMenu(
    aura::Window* root_window) {
  return NULL;
}

ash::LauncherMenuModel*
WindowWatcherLauncherItemDelegate::CreateApplicationMenu(
    int event_flags) {
  return NULL;
}

bool WindowWatcherLauncherItemDelegate::IsDraggable() {
  return true;
}

bool WindowWatcherLauncherItemDelegate::ShouldShowTooltip() {
  return true;
}

}  // namespace shell
}  // namespace ash
