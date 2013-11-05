// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_launcher_item_delegate.h"

#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

TestLauncherItemDelegate::TestLauncherItemDelegate(aura::Window* window)
    : window_(window) {
}

TestLauncherItemDelegate::~TestLauncherItemDelegate() {
}

bool TestLauncherItemDelegate::ItemSelected(const ui::Event& event) {
  if (window_) {
    if (window_->type() == aura::client::WINDOW_TYPE_PANEL)
      ash::wm::MoveWindowToEventRoot(window_, event);
    window_->Show();
    ash::wm::ActivateWindow(window_);
  }
  return false;
}

base::string16 TestLauncherItemDelegate::GetTitle() {
  return window_ ? window_->title() : base::string16();
}

ui::MenuModel* TestLauncherItemDelegate::CreateContextMenu(
    aura::Window* root_window) {
  return NULL;
}

ash::LauncherMenuModel* TestLauncherItemDelegate::CreateApplicationMenu(
    int event_flags) {
  return NULL;
}

bool TestLauncherItemDelegate::IsDraggable() {
  return true;
}

bool TestLauncherItemDelegate::ShouldShowTooltip() {
  return true;
}

}  // namespace test
}  // namespace ash
