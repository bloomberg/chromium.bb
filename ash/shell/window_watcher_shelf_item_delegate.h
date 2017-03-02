// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_WINDOW_WATCHER_SHELF_ITEM_DELEGATE_H_
#define ASH_SHELL_WINDOW_WATCHER_SHELF_ITEM_DELEGATE_H_

#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace ash {
namespace shell {

class WindowWatcher;

// ShelfItemDelegate implementation used by WindowWatcher.
class WindowWatcherShelfItemDelegate : public ShelfItemDelegate {
 public:
  WindowWatcherShelfItemDelegate(ShelfID id, WindowWatcher* watcher);
  ~WindowWatcherShelfItemDelegate() override;

  // ShelfItemDelegate:
  ShelfAction ItemSelected(ui::EventType event_type,
                           int event_flags,
                           int64_t display_id,
                           ShelfLaunchSource source) override;
  ShelfAppMenuItemList GetAppMenuItems(int event_flags) override;
  void ExecuteCommand(uint32_t command_id, int event_flags) override;
  void Close() override;

 private:
  ShelfID id_;
  WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(WindowWatcherShelfItemDelegate);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_WINDOW_WATCHER_SHELF_ITEM_DELEGATE_H_
