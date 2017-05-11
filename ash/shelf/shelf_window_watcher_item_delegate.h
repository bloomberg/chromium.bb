// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
#define ASH_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/macros.h"

namespace ash {

class WmWindow;

// ShelfItemDelegate for the items created by ShelfWindowWatcher, for example:
// The Chrome OS settings window, task manager window, and panel windows.
class ShelfWindowWatcherItemDelegate : public ShelfItemDelegate {
 public:
  ShelfWindowWatcherItemDelegate(const ShelfID& id, WmWindow* window);
  ~ShelfWindowWatcherItemDelegate() override;

 private:
  // ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback) override;
  void ExecuteCommand(uint32_t command_id, int32_t event_flags) override;
  void Close() override;

  // The window associated with this item. Not owned.
  WmWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcherItemDelegate);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
