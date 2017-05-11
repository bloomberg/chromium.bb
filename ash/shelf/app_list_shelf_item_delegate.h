// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_APP_LIST_SHELF_ITEM_DELEGATE_H_
#define ASH_SHELF_APP_LIST_SHELF_ITEM_DELEGATE_H_

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/macros.h"

namespace ash {
class ShelfModel;

// ShelfItemDelegate for TYPE_APP_LIST.
class AppListShelfItemDelegate : public ShelfItemDelegate {
 public:
  // Initializes the app list item in the shelf data model and creates an
  // AppListShelfItemDelegate which will be owned by |model|.
  static void CreateAppListItemAndDelegate(ShelfModel* model);

  AppListShelfItemDelegate();
  ~AppListShelfItemDelegate() override;

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback) override;
  void ExecuteCommand(uint32_t command_id, int32_t event_flags) override;
  void Close() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListShelfItemDelegate);
};

}  // namespace ash

#endif  // ASH_SHELF_APP_LIST_SHELF_ITEM_DELEGATE_H_
