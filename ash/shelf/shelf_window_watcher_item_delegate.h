// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
#define ASH_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_

#include "ash/shelf/shelf_item_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

// ShelfItemDelegate for the items created by ShelfWindowWatcher.
class ShelfWindowWatcherItemDelegate : public ShelfItemDelegate {
 public:
  explicit ShelfWindowWatcherItemDelegate(aura::Window* window);

  virtual ~ShelfWindowWatcherItemDelegate();

  // Closes the window associated with this item.
  void Close();

 private:
  // ShelfItemDelegate overrides:
  virtual bool ItemSelected(const ui::Event& event) OVERRIDE;
  virtual base::string16 GetTitle() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(aura::Window* root_window) OVERRIDE;
  virtual ShelfMenuModel* CreateApplicationMenu(int event_flags) OVERRIDE;
  virtual bool IsDraggable() OVERRIDE;
  virtual bool ShouldShowTooltip() OVERRIDE;

  // Stores a Window associated with this item. Not owned.
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcherItemDelegate);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
