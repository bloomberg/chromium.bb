// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_launcher_delegate.h"

#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_util.h"
#include "ash/wm/window_util.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
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
  AddLauncherItem(window, STATUS_CLOSED);
}

void TestLauncherDelegate::AddLauncherItem(
    aura::Window* window,
    LauncherItemStatus status) {
  ash::LauncherItem item;
  if (window->type() == aura::client::WINDOW_TYPE_PANEL)
    item.type = ash::TYPE_APP_PANEL;
  else
    item.type = ash::TYPE_TABBED;
  DCHECK(window_to_id_.find(window) == window_to_id_.end());
  window_to_id_[window] = model_->next_id();
  item.status = status;
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

void TestLauncherDelegate::OnBrowserShortcutClicked(int event_flags) {
}

void TestLauncherDelegate::ItemClicked(const ash::LauncherItem& item,
                                       const ui::Event& event) {
  aura::Window* window = GetWindowByID(item.id);
  launcher::MoveToEventRootIfPanel(window, event);
  window->Show();
  ash::wm::ActivateWindow(window);
}

int TestLauncherDelegate::GetBrowserShortcutResourceId() {
  return IDR_AURA_LAUNCHER_BROWSER_SHORTCUT;
}

string16 TestLauncherDelegate::GetTitle(const ash::LauncherItem& item) {
  aura::Window* window = GetWindowByID(item.id);
  return window ? window->title() : string16();
}

ui::MenuModel* TestLauncherDelegate::CreateContextMenu(
    const ash::LauncherItem& item,
    aura::RootWindow* root) {
  return NULL;
}

ash::LauncherMenuModel* TestLauncherDelegate::CreateApplicationMenu(
    const ash::LauncherItem& item,
    int event_flags) {
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

bool TestLauncherDelegate::IsDraggable(const ash::LauncherItem& item) {
  return true;
}

bool TestLauncherDelegate::ShouldShowTooltip(const ash::LauncherItem& item) {
  return true;
}

}  // namespace test
}  // namespace ash
