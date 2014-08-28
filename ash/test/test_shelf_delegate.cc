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

void TestShelfDelegate::AddShelfItem(aura::Window* window) {
  AddShelfItem(window, STATUS_CLOSED);
}

void TestShelfDelegate::AddShelfItem(aura::Window* window,
                                     ShelfItemStatus status) {
  ShelfItem item;
  if (window->type() == ui::wm::WINDOW_TYPE_PANEL)
    item.type = TYPE_APP_PANEL;
  else
    item.type = TYPE_PLATFORM_APP;
  ShelfID id = model_->next_id();
  item.status = status;
  model_->Add(item);
  window->AddObserver(this);

  ShelfItemDelegateManager* manager =
      Shell::GetInstance()->shelf_item_delegate_manager();
  // |manager| owns TestShelfItemDelegate.
  scoped_ptr<ShelfItemDelegate> delegate(new TestShelfItemDelegate(window));
  manager->SetShelfItemDelegate(id, delegate.Pass());
  SetShelfIDForWindow(id, window);
}

void TestShelfDelegate::RemoveShelfItemForWindow(aura::Window* window) {
  ShelfID id = GetShelfIDForWindow(window);
  if (id == 0)
    return;
  int index = model_->ItemIndexByID(id);
  DCHECK_NE(-1, index);
  model_->RemoveItemAt(index);
  window->RemoveObserver(this);
}

void TestShelfDelegate::OnWindowDestroying(aura::Window* window) {
  RemoveShelfItemForWindow(window);
}

void TestShelfDelegate::OnWindowHierarchyChanging(
      const HierarchyChangeParams& params) {
  // The window may be legitimately reparented while staying open if it moves
  // to another display or container. If the window does not have a new parent
  // then remove the shelf item.
  if (!params.new_parent)
    RemoveShelfItemForWindow(params.target);
}

void TestShelfDelegate::OnShelfCreated(Shelf* shelf) {
}

void TestShelfDelegate::OnShelfDestroyed(Shelf* shelf) {
}

ShelfID TestShelfDelegate::GetShelfIDForAppID(const std::string& app_id) {
  return 0;
}

const std::string& TestShelfDelegate::GetAppIDForShelfID(ShelfID id) {
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
