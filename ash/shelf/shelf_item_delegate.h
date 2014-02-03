// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_ITEM_DELEGATE_H_
#define ASH_SHELF_SHELF_ITEM_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace aura {
class Window;
}

namespace ui {
class Event;
class MenuModel;
}

namespace ash {

class ShelfMenuModel;

// Delegate for the ShelfItem.
class ASH_EXPORT ShelfItemDelegate {
 public:
  virtual ~ShelfItemDelegate() {}

  // Invoked when the user clicks on a window entry in the launcher.
  // |event| is the click event. The |event| is dispatched by a view
  // and has an instance of |views::View| as the event target
  // but not |aura::Window|. If the |event| is of type KeyEvent, it is assumed
  // that this was triggered by keyboard action (Alt+<number>) and special
  // handling might happen.
  // Returns true if a new item was created.
  virtual bool ItemSelected(const ui::Event& event) = 0;

  // Returns the title to display.
  virtual base::string16 GetTitle() = 0;

  // Returns the context menumodel for the specified item on
  // |root_window|.  Return NULL if there should be no context
  // menu. The caller takes ownership of the returned model.
  virtual ui::MenuModel* CreateContextMenu(aura::Window* root_window) = 0;

  // Returns the application menu model for the specified item. There are three
  // possible return values:
  //  - A return of NULL indicates that no menu is wanted for this item.
  //  - A return of a menu with one item means that only the name of the
  //    application/item was added and there are no active applications.
  //    Note: This is useful for hover menus which also show context help.
  //  - A list containing the title and the active list of items.
  // The caller takes ownership of the returned model.
  // |event_flags| specifies the flags of the event which triggered this menu.
  virtual ShelfMenuModel* CreateApplicationMenu(int event_flags) = 0;

  // Whether the launcher item is draggable.
  virtual bool IsDraggable() = 0;

  // Returns true if a tooltip should be shown.
  virtual bool ShouldShowTooltip() = 0;

  // Closes all windows associated with this item.
  virtual void Close() = 0;

};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_ITEM_DELEGATE_H_
