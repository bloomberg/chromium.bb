// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shelf_delegate.h"

#include "ash/shelf/shelf_model.h"
#include "ash/shell.h"
#include "base/strings/string_util.h"

namespace ash {
namespace test {

TestShelfDelegate* TestShelfDelegate::instance_ = nullptr;

TestShelfDelegate::TestShelfDelegate() {
  CHECK(!instance_);
  instance_ = this;
}

TestShelfDelegate::~TestShelfDelegate() {
  instance_ = nullptr;
}

ShelfID TestShelfDelegate::GetShelfIDForAppID(const std::string& app_id) {
  // Get shelf id for |app_id| and an empty |launch_id|.
  return GetShelfIDForAppIDAndLaunchID(app_id, std::string());
}

ShelfID TestShelfDelegate::GetShelfIDForAppIDAndLaunchID(
    const std::string& app_id,
    const std::string& launch_id) {
  for (const ShelfItem& item : Shell::Get()->shelf_model()->items()) {
    // Ash's ShelfWindowWatcher handles app panel windows separately.
    if (item.type != TYPE_APP_PANEL && item.app_launch_id.app_id() == app_id &&
        item.app_launch_id.launch_id() == launch_id) {
      return item.id;
    }
  }
  return kInvalidShelfID;
}

const std::string& TestShelfDelegate::GetAppIDForShelfID(ShelfID id) {
  ShelfModel* model = Shell::Get()->shelf_model();
  ShelfItems::const_iterator item = model->ItemByID(id);
  return item != model->items().end() ? item->app_launch_id.app_id()
                                      : base::EmptyString();
}

void TestShelfDelegate::PinAppWithID(const std::string& app_id) {
  // If the app is already pinned, do nothing and return.
  if (IsAppPinned(app_id))
    return;

  // Convert an existing item to be pinned, or create a new pinned item.
  ShelfModel* model = Shell::Get()->shelf_model();
  const int index = model->ItemIndexByID(GetShelfIDForAppID(app_id));
  if (index >= 0) {
    ShelfItem item = model->items()[index];
    DCHECK_EQ(item.type, TYPE_APP);
    DCHECK(!item.pinned_by_policy);
    item.type = TYPE_PINNED_APP;
    model->Set(index, item);
  } else if (!app_id.empty()) {
    ShelfItem item;
    item.type = TYPE_PINNED_APP;
    item.app_launch_id = AppLaunchId(app_id);
    model->Add(item);
  }
}

bool TestShelfDelegate::IsAppPinned(const std::string& app_id) {
  ShelfID shelf_id = GetShelfIDForAppID(app_id);
  ShelfModel* model = Shell::Get()->shelf_model();
  ShelfItems::const_iterator item = model->ItemByID(shelf_id);
  return item != model->items().end() && item->type == TYPE_PINNED_APP;
}

void TestShelfDelegate::UnpinAppWithID(const std::string& app_id) {
  // If the app is already not pinned, do nothing and return.
  if (!IsAppPinned(app_id))
    return;

  // Remove the item if it is closed, or mark it as unpinned.
  ShelfModel* model = Shell::Get()->shelf_model();
  const int index = model->ItemIndexByID(GetShelfIDForAppID(app_id));
  ShelfItem item = model->items()[index];
  DCHECK_EQ(item.type, TYPE_PINNED_APP);
  DCHECK(!item.pinned_by_policy);
  if (item.status == STATUS_CLOSED) {
    model->RemoveItemAt(index);
  } else {
    item.type = TYPE_APP;
    model->Set(index, item);
  }
}

}  // namespace test
}  // namespace ash
