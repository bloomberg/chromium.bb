// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_launcher_delegate.h"

#include "ash/launcher/launcher_item_delegate_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/test/test_launcher_item_delegate.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

TestLauncherDelegate* TestLauncherDelegate::instance_ = NULL;

TestLauncherDelegate::TestLauncherDelegate(ShelfModel* model)
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
    item.type = ash::TYPE_PLATFORM_APP;
  LauncherID id = model_->next_id();
  item.status = status;
  model_->Add(item);
  window->AddObserver(this);

  ash::LauncherItemDelegateManager* manager =
      ash::Shell::GetInstance()->launcher_item_delegate_manager();
  // |manager| owns TestLauncherItemDelegate.
  scoped_ptr<LauncherItemDelegate> delegate(
      new TestLauncherItemDelegate(window));
  manager->SetLauncherItemDelegate(id, delegate.Pass());
  SetLauncherIDForWindow(id, window);
}

void TestLauncherDelegate::RemoveLauncherItemForWindow(aura::Window* window) {
  ash::LauncherID id = GetLauncherIDForWindow(window);
  if (id == 0)
    return;
  int index = model_->ItemIndexByID(id);
  DCHECK_NE(-1, index);
  model_->RemoveItemAt(index);
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

void TestLauncherDelegate::OnLauncherCreated(Launcher* launcher) {
}

void TestLauncherDelegate::OnLauncherDestroyed(Launcher* launcher) {
}

LauncherID TestLauncherDelegate::GetLauncherIDForAppID(
    const std::string& app_id) {
  return 0;
}

const std::string& TestLauncherDelegate::GetAppIDForLauncherID(LauncherID id) {
  return EmptyString();
}

void TestLauncherDelegate::PinAppWithID(const std::string& app_id) {
}

bool TestLauncherDelegate::CanPin() const {
  return true;
}

bool TestLauncherDelegate::IsAppPinned(const std::string& app_id) {
  return false;
}

void TestLauncherDelegate::UnpinAppWithID(const std::string& app_id) {
}

}  // namespace test
}  // namespace ash
