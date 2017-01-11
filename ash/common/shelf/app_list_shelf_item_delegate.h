// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_APP_LIST_SHELF_ITEM_DELEGATE_H_
#define ASH_COMMON_SHELF_APP_LIST_SHELF_ITEM_DELEGATE_H_

#include "ash/common/shelf/shelf_item_delegate.h"
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
  ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override;
  ShelfMenuModel* CreateApplicationMenu(int event_flags) override;
  void Close() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListShelfItemDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_APP_LIST_SHELF_ITEM_DELEGATE_H_
