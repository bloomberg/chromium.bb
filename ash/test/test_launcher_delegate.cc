// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_launcher_delegate.h"

#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_util.h"
#include "ash/wm/window_util.h"
#include "base/strings/utf_string_conversions.h"
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
  window->AddObserver(this);
}

void TestLauncherDelegate::RemoveLauncherItemForWindow(aura::Window* window) {
  ash::LauncherID id = GetIDByWindow(window);
  if (id == 0)
    return;
  int index = model_->ItemIndexByID(id);
  DCHECK_NE(-1, index);
  model_->RemoveItemAt(index);
  window_to_id_.erase(window);
  window->RemoveObserver(this);
}

void TestLauncherDelegate::OnWindowDestroying(aura::Window* window) {
  RemoveLauncherItemForWindow(window);
}

void TestLauncherDelegate::OnWindowHierarchyChanging(
      const HierarchyChangeParams& params) {
  // The window may be legitimately reparented while staying open if it moves
  // to another display or container. If the window does not have a new parent
  // then remove the launcher item.
  if (!params.new_parent)
    RemoveLauncherItemForWindow(params.target);
}

void TestLauncherDelegate::ItemSelected(const ash::LauncherItem& item,
                                       const ui::Event& event) {
  aura::Window* window = GetWindowByID(item.id);
  if (window->type() == aura::client::WINDOW_TYPE_PANEL)
    ash::wm::MoveWindowToEventRoot(window, event);
  window->Show();
  ash::wm::ActivateWindow(window);
}

base::string16 TestLauncherDelegate::GetTitle(const ash::LauncherItem& item) {
  aura::Window* window = GetWindowByID(item.id);
  return window ? window->title() : base::string16();
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

void TestLauncherDelegate::OnLauncherCreated(Launcher* launcher) {
}

void TestLauncherDelegate::OnLauncherDestroyed(Launcher* launcher) {
}

bool TestLauncherDelegate::IsPerAppLauncher() {
  return true;
}

LauncherID TestLauncherDelegate::GetLauncherIDForAppID(
    const std::string& app_id) {
  return 0;
}

void TestLauncherDelegate::PinAppWithID(const std::string& app_id) {
}

bool TestLauncherDelegate::IsAppPinned(const std::string& app_id) {
  return false;
}

void TestLauncherDelegate::UnpinAppsWithID(const std::string& app_id) {
}

}  // namespace test
}  // namespace ash
