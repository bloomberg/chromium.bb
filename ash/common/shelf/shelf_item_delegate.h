// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_ITEM_DELEGATE_H_
#define ASH_COMMON_SHELF_SHELF_ITEM_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_application_menu_item.h"

namespace ui {
class Event;
}

namespace ash {

// Delegate for the ShelfItem.
class ASH_EXPORT ShelfItemDelegate {
 public:
  // The return type for the ShelfItemDelegate::ItemSelected method.
  enum PerformedAction {
    // No action was taken.
    kNoAction,
    // A new window was created.
    kNewWindowCreated,
    // An existing window which was not currently active was activated.
    kExistingWindowActivated,
    // The currently active window was minimized.
    kExistingWindowMinimized,
    // The app list launcher menu was shown.
    kAppListMenuShown,
  };

  virtual ~ShelfItemDelegate() {}

  // Invoked when the user clicks on a window entry in the launcher.
  // |event| is the click event. The |event| is dispatched by a view
  // and has an instance of |views::View| as the event target
  // but not |aura::Window|. If the |event| is of type KeyEvent, it is assumed
  // that this was triggered by keyboard action (Alt+<number>) and special
  // handling might happen.
  // Returns the action performed by selecting the item.
  virtual PerformedAction ItemSelected(const ui::Event& event) = 0;

  // Returns any application menu items that should appear for this shelf item.
  // |event_flags| specifies the flags of the event which triggered this menu.
  virtual ShelfAppMenuItemList GetAppMenuItems(int event_flags) = 0;

  // Closes all windows associated with this item.
  virtual void Close() = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_ITEM_DELEGATE_H_
