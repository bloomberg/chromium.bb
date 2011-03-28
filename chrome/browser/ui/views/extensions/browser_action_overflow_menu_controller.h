// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BROWSER_ACTION_OVERFLOW_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BROWSER_ACTION_OVERFLOW_MENU_CONTROLLER_H_
#pragma once

#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "views/controls/menu/menu_delegate.h"

class BrowserActionsContainer;
class BrowserActionView;
class ExtensionContextMenuModel;

namespace views {
class Menu2;
}

// This class handles the overflow menu for browser actions (showing the menu,
// drag and drop, etc). This class manages its own lifetime.
class BrowserActionOverflowMenuController : public views::MenuDelegate {
 public:
  // The observer is notified prior to the menu being deleted.
  class Observer {
   public:
    virtual void NotifyMenuDeleted(
        BrowserActionOverflowMenuController* controller) = 0;
  };

  BrowserActionOverflowMenuController(
      BrowserActionsContainer* owner,
      views::MenuButton* menu_button,
      const std::vector<BrowserActionView*>& views,
      int start_index);

  void set_observer(Observer* observer) { observer_ = observer; }

  // Shows the overflow menu.
  bool RunMenu(gfx::NativeWindow window, bool for_drop);

  // Closes the overflow menu (and its context menu if open as well).
  void CancelMenu();

  // Overridden from views::MenuDelegate:
  virtual void ExecuteCommand(int id);
  virtual bool ShowContextMenu(views::MenuItemView* source,
                               int id,
                               const gfx::Point& p,
                               bool is_mouse_gesture);
  virtual void DropMenuClosed(views::MenuItemView* menu);
  // These drag functions offer support for dragging icons into the overflow
  // menu.
  virtual bool GetDropFormats(
      views::MenuItemView* menu,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats);
  virtual bool AreDropTypesRequired(views::MenuItemView* menu);
  virtual bool CanDrop(views::MenuItemView* menu, const ui::OSExchangeData& data);
  virtual int GetDropOperation(views::MenuItemView* item,
                               const views::DropTargetEvent& event,
                               DropPosition* position);
  virtual int OnPerformDrop(views::MenuItemView* menu,
                            DropPosition position,
                            const views::DropTargetEvent& event);
  // These three drag functions offer support for dragging icons out of the
  // overflow menu.
  virtual bool CanDrag(views::MenuItemView* menu);
  virtual void WriteDragData(views::MenuItemView* sender, ui::OSExchangeData* data);
  virtual int GetDragOperations(views::MenuItemView* sender);

 private:
  // This class manages its own lifetime.
  virtual ~BrowserActionOverflowMenuController();

  // Converts a menu item |id| into a BrowserActionView by adding the |id| value
  // to the number of visible views (according to the container owner). If
  // |index| is specified, it will point to the absolute index of the view.
  BrowserActionView* ViewForId(int id, size_t* index);

  // A pointer to the browser action container that owns the overflow menu.
  BrowserActionsContainer* owner_;

  // The observer, may be null.
  Observer* observer_;

  // A pointer to the overflow menu button that we are showing the menu for.
  views::MenuButton* menu_button_;

  // The overflow menu for the menu button.
  scoped_ptr<views::MenuItemView> menu_;

  // The views vector of all the browser actions the container knows about. We
  // won't show all items, just the one starting at |start_index| and above.
  const std::vector<BrowserActionView*>* views_;

  // The index into the BrowserActionView vector, indicating where to start
  // picking browser actions to draw.
  int start_index_;

  // Whether this controller is being used for drop.
  bool for_drop_;

  // The browser action context menu and model.
  scoped_refptr<ExtensionContextMenuModel> context_menu_contents_;
  scoped_ptr<views::Menu2> context_menu_menu_;

  friend class DeleteTask<BrowserActionOverflowMenuController>;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionOverflowMenuController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BROWSER_ACTION_OVERFLOW_MENU_CONTROLLER_H_
