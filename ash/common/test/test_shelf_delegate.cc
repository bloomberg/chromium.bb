// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_shelf_delegate.h"

#include <utility>

#include "ash/common/shelf/shelf_model.h"
#include "ash/common/test/test_shelf_item_delegate.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "base/memory/ptr_util.h"

namespace ash {
namespace test {

TestShelfDelegate* TestShelfDelegate::instance_ = nullptr;

TestShelfDelegate::TestShelfDelegate(ShelfModel* model) : model_(model) {
  CHECK(!instance_);
  instance_ = this;
}

TestShelfDelegate::~TestShelfDelegate() {
  instance_ = nullptr;
}

void TestShelfDelegate::AddShelfItem(WmWindow* window) {
  AddShelfItem(window, STATUS_CLOSED);
}

void TestShelfDelegate::AddShelfItem(WmWindow* window,
                                     const std::string& app_id) {
  AddShelfItem(window, STATUS_CLOSED);
  ShelfID shelf_id = window->GetIntProperty(WmWindowProperty::SHELF_ID);
  AddShelfIDToAppIDMapping(shelf_id, app_id);
}

void TestShelfDelegate::AddShelfItem(WmWindow* window, ShelfItemStatus status) {
  ShelfItem item;
  if (window->GetType() == ui::wm::WINDOW_TYPE_PANEL)
    item.type = TYPE_APP_PANEL;
  else
    item.type = TYPE_APP;
  ShelfID id = model_->next_id();
  item.status = status;
  model_->Add(item);
  window->AddObserver(this);

  model_->SetShelfItemDelegate(id,
                               base::MakeUnique<TestShelfItemDelegate>(window));
  window->SetIntProperty(WmWindowProperty::SHELF_ID, id);
}

void TestShelfDelegate::RemoveShelfItemForWindow(WmWindow* window) {
  ShelfID shelf_id = window->GetIntProperty(WmWindowProperty::SHELF_ID);
  if (shelf_id == 0)
    return;
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_NE(-1, index);
  model_->RemoveItemAt(index);
  window->RemoveObserver(this);
  if (HasShelfIDToAppIDMapping(shelf_id)) {
    const std::string& app_id = GetAppIDForShelfID(shelf_id);
    if (IsAppPinned(app_id))
      UnpinAppWithID(app_id);
    if (HasShelfIDToAppIDMapping(shelf_id))
      RemoveShelfIDToAppIDMapping(shelf_id);
  }
}

void TestShelfDelegate::OnWindowDestroying(WmWindow* window) {
  RemoveShelfItemForWindow(window);
}

void TestShelfDelegate::OnWindowTreeChanging(WmWindow* window,
                                             const TreeChangeParams& params) {
  // The window may be legitimately reparented while staying open if it moves
  // to another display or container. If the window does not have a new parent
  // then remove the shelf item.
  if (!params.new_parent)
    RemoveShelfItemForWindow(params.target);
}

ShelfID TestShelfDelegate::GetShelfIDForAppID(const std::string& app_id) {
  for (auto const& iter : shelf_id_to_app_id_map_) {
    if (iter.second == app_id)
      return iter.first;
  }
  return 0;
}

ShelfID TestShelfDelegate::GetShelfIDForAppIDAndLaunchID(
    const std::string& app_id,
    const std::string& launch_id) {
  return GetShelfIDForAppID(app_id);
}

bool TestShelfDelegate::HasShelfIDToAppIDMapping(ShelfID id) const {
  return shelf_id_to_app_id_map_.find(id) != shelf_id_to_app_id_map_.end();
}

const std::string& TestShelfDelegate::GetAppIDForShelfID(ShelfID id) {
  DCHECK_GT(shelf_id_to_app_id_map_.count(id), 0u);
  return shelf_id_to_app_id_map_[id];
}

void TestShelfDelegate::PinAppWithID(const std::string& app_id) {
  pinned_apps_.insert(app_id);
}

bool TestShelfDelegate::IsAppPinned(const std::string& app_id) {
  return pinned_apps_.find(app_id) != pinned_apps_.end();
}

void TestShelfDelegate::UnpinAppWithID(const std::string& app_id) {
  pinned_apps_.erase(app_id);
}

void TestShelfDelegate::AddShelfIDToAppIDMapping(ShelfID shelf_id,
                                                 const std::string& app_id) {
  shelf_id_to_app_id_map_[shelf_id] = app_id;
}

void TestShelfDelegate::RemoveShelfIDToAppIDMapping(ShelfID shelf_id) {
  shelf_id_to_app_id_map_.erase(shelf_id);
}

}  // namespace test
}  // namespace ash
