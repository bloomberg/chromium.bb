// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
#define ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_

#include "ash/common/shelf/shelf_item_delegate.h"
#include "base/macros.h"

namespace ash {

class WmWindow;

// ShelfItemDelegate for the items created by ShelfWindowWatcher.
class ShelfWindowWatcherItemDelegate : public ShelfItemDelegate {
 public:
  explicit ShelfWindowWatcherItemDelegate(WmWindow* window);
  ~ShelfWindowWatcherItemDelegate() override;

 private:
  // ShelfItemDelegate overrides:
  ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override;
  base::string16 GetTitle() override;
  ShelfMenuModel* CreateApplicationMenu(int event_flags) override;
  bool IsDraggable() override;
  bool CanPin() const override;
  bool ShouldShowTooltip() override;
  void Close() override;

  // The window associated with this item. Not owned.
  WmWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcherItemDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
