// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shelf_delegate.h"

#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/test/test_shelf_item_delegate.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

TestShelfDelegate* TestShelfDelegate::instance_ = NULL;

TestShelfDelegate::TestShelfDelegate(ShelfModel* model)
    : model_(model) {
  CHECK(!instance_);
  instance_ = this;
}

TestShelfDelegate::~TestShelfDelegate() {
  instance_ = NULL;
}

void TestShelfDelegate::AddLauncherItem(aura::Window* window) {
  AddLauncherItem(window, STATUS_CLOSED);
}

void TestShelfDelegate::AddLauncherItem(aura::Window* window,
                                        LauncherItemStatus status) {
  LauncherItem item;
  if (window->type() == aura::client::WINDOW_TYPE_PANEL)
    item.type = TYPE_APP_PANEL;
  else
    item.type = TYPE_PLATFORM_APP;
  LauncherID id = model_->next_id();
  item.status = status;
  model_->Add(item);
  window->AddObserver(this);

  ShelfItemDelegateManager* manager =
      Shell::GetInstance()->shelf_item_delegate_manager();
  // |manager| owns TestShelfItemDelegate.
  scoped_ptr<ShelfItemDelegate> delegate(new TestShelfItemDelegate(window));
  manager->SetShelfItemDelegate(id, delegate.Pass());
  SetLauncherIDForWindow(id, window);
}

void TestShelfDelegate::RemoveLauncherItemForWindow(aura::Window* window) {
  LauncherID id = GetLauncherIDForWindow(window);
  if (id == 0)
    return;
  int index = model_->ItemIndexByID(id);
  DCHECK_NE(-1, index);
  model_->RemoveItemAt(index);
  window->RemoveObserver(this);
}

void TestShelfDelegate::OnWindowDestroying(aura::Window* window) {
  RemoveLauncherItemForWindow(window);
}

void TestShelfDelegate::OnWindowHierarchyChanging(
      const HierarchyChangeParams& params) {
  // The window may be legitimately reparented while staying open if it moves
  // to another display or container. If the window does not have a new parent
  // then remove the launcher item.
  if (!params.new_parent)
    RemoveLauncherItemForWindow(params.target);
}

void TestShelfDelegate::OnLauncherCreated(Launcher* launcher) {
}

void TestShelfDelegate::OnLauncherDestroyed(Launcher* launcher) {
}

LauncherID TestShelfDelegate::GetLauncherIDForAppID(const std::string& app_id) {
  return 0;
}

const std::string& TestShelfDelegate::GetAppIDForLauncherID(LauncherID id) {
  return base::EmptyString();
}

void TestShelfDelegate::PinAppWithID(const std::string& app_id) {
}

bool TestShelfDelegate::CanPin() const {
  return true;
}

bool TestShelfDelegate::IsAppPinned(const std::string& app_id) {
  return false;
}

void TestShelfDelegate::UnpinAppWithID(const std::string& app_id) {
}

}  // namespace test
}  // namespace ash
