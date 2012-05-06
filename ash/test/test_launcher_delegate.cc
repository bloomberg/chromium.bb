// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_launcher_delegate.h"

#include "ash/launcher/launcher_model.h"
#include "ash/wm/window_util.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

TestLauncherDelegate* TestLauncherDelegate::instance_ = NULL;

TestLauncherDelegate::TestLauncherDelegate(LauncherModel* model)
    : model_(model) {
  CHECK(!instance_);
  instance_ = this;
}

TestLauncherDelegate::~TestLauncherDelegate() {
  instance_ = NULL;
}

void TestLauncherDelegate::AddLauncherItem(aura::Window* window) {
  ash::LauncherItem item;
  item.type = ash::TYPE_TABBED;
  DCHECK(window_to_id_.find(window) == window_to_id_.end());
  window_to_id_[window] = model_->next_id();
  item.image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  item.image.allocPixels();
  model_->Add(item);
  if (observed_windows_.find(window->parent()) == observed_windows_.end()) {
    window->parent()->AddObserver(this);
    observed_windows_.insert(window->parent());
  }
}

void TestLauncherDelegate::OnWillRemoveWindow(aura::Window* window) {
  ash::LauncherID id = GetIDByWindow(window);
  if (id == 0)
    return;
  int index = model_->ItemIndexByID(id);
  DCHECK_NE(-1, index);
  model_->RemoveItemAt(index);
  window_to_id_.erase(window);
  ObservedWindows::iterator it = observed_windows_.find(window->parent());
  if (it != observed_windows_.end()) {
    window->parent()->RemoveObserver(this);
    observed_windows_.erase(it);
  }
}

void TestLauncherDelegate::CreateNewTab() {
}

void TestLauncherDelegate::CreateNewWindow() {
}

void TestLauncherDelegate::ItemClicked(const ash::LauncherItem& item,
                                       int event_flags) {
  aura::Window* window = GetWindowByID(item.id);
  window->Show();
  ash::wm::ActivateWindow(window);
}

int TestLauncherDelegate::GetBrowserShortcutResourceId() {
  return IDR_AURA_LAUNCHER_BROWSER_SHORTCUT;
}

string16 TestLauncherDelegate::GetTitle(const ash::LauncherItem& item) {
  return GetWindowByID(item.id)->title();
}

ui::MenuModel* TestLauncherDelegate::CreateContextMenu(
    const ash::LauncherItem& item) {
  return NULL;
}

ui::MenuModel* TestLauncherDelegate::CreateContextMenuForLauncher() {
  return NULL;
}

ash::LauncherID TestLauncherDelegate::GetIDByWindow(aura::Window* window) {
  WindowToID::const_iterator found = window_to_id_.find(window);
  if (found == window_to_id_.end())
    return 0;
  return found->second;
}

aura::Window* TestLauncherDelegate::GetWindowByID(ash::LauncherID id) {
  for (WindowToID::const_iterator it = window_to_id_.begin();
      it != window_to_id_.end();
      it++) {
    if (it->second == id)
      return it->first;
  }
  return NULL;
}

}  // namespace test
}  // namespace ash
