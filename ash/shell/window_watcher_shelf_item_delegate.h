// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_WINDOW_WATCHER_SHELF_ITEM_DELEGATE_H_
#define ASH_SHELL_WINDOW_WATCHER_SHELF_ITEM_DELEGATE_H_

#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_item_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {
namespace shell {

class WindowWatcher;

// ShelfItemDelegate implementation used by WindowWatcher.
class WindowWatcherShelfItemDelegate : public ShelfItemDelegate {
 public:
  WindowWatcherShelfItemDelegate(ShelfID id, WindowWatcher* watcher);
  ~WindowWatcherShelfItemDelegate() override;

  // ShelfItemDelegate:
  bool ItemSelected(const ui::Event& event) override;
  base::string16 GetTitle() override;
  ui::MenuModel* CreateContextMenu(aura::Window* root_window) override;
  ShelfMenuModel* CreateApplicationMenu(int event_flags) override;
  bool IsDraggable() override;
  bool ShouldShowTooltip() override;
  void Close() override;

 private:
  ShelfID id_;
  WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(WindowWatcherShelfItemDelegate);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_WINDOW_WATCHER_SHELF_ITEM_DELEGATE_H_
