// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
#pragma once

#include <set>

#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/menu_item_view.h"

class BookmarkBarView;
class BookmarkMenuDelegate;
class BookmarkNode;
class PageNavigator;
class Profile;

namespace ui {
class OSExchangeData;
}  // namespace ui

namespace views {
class MenuButton;
class MenuRunner;
class Widget;
}  // namespace views

// BookmarkMenuController is responsible for showing a menu of bookmarks,
// each item in the menu represents a bookmark.
// BookmarkMenuController deletes itself as necessary, although the menu can
// be explicitly hidden by way of the Cancel method.
class BookmarkMenuController : public BaseBookmarkModelObserver,
                               public views::MenuDelegate {
 public:
  // The observer is notified prior to the menu being deleted.
  class Observer {
   public:
    virtual void BookmarkMenuDeleted(BookmarkMenuController* controller) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Creates a BookmarkMenuController showing the children of |node| starting
  // at |start_child_index|.
  BookmarkMenuController(Profile* profile,
                         PageNavigator* page_navigator,
                         views::Widget* parent,
                         const BookmarkNode* node,
                         int start_child_index);

  void RunMenuAt(BookmarkBarView* bookmark_bar, bool for_drop);

  // Hides the menu.
  void Cancel();

  // Returns the node the menu is showing for.
  const BookmarkNode* node() const { return node_; }

  // Returns the menu.
  views::MenuItemView* menu() const;

  // Returns the context menu, or NULL if the context menu isn't showing.
  views::MenuItemView* context_menu() const;

  // Sets the page navigator.
  void SetPageNavigator(PageNavigator* navigator);

  void set_observer(Observer* observer) { observer_ = observer; }

  // MenuDelegate methods.
  virtual string16 GetTooltipText(int id, const gfx::Point& p) const OVERRIDE;
  virtual bool IsTriggerableEvent(views::MenuItemView* view,
                                  const views::MouseEvent& e) OVERRIDE;
  virtual void ExecuteCommand(int id, int mouse_event_flags) OVERRIDE;
  virtual bool GetDropFormats(
      views::MenuItemView* menu,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool AreDropTypesRequired(views::MenuItemView* menu) OVERRIDE;
  virtual bool CanDrop(views::MenuItemView* menu,
                       const ui::OSExchangeData& data) OVERRIDE;
  virtual int GetDropOperation(views::MenuItemView* item,
                               const views::DropTargetEvent& event,
                               DropPosition* position) OVERRIDE;
  virtual int OnPerformDrop(views::MenuItemView* menu,
                            DropPosition position,
                            const views::DropTargetEvent& event) OVERRIDE;
  virtual bool ShowContextMenu(views::MenuItemView* source,
                               int id,
                               const gfx::Point& p,
                               bool is_mouse_gesture) OVERRIDE;
  virtual void DropMenuClosed(views::MenuItemView* menu) OVERRIDE;
  virtual bool CanDrag(views::MenuItemView* menu) OVERRIDE;
  virtual void WriteDragData(views::MenuItemView* sender,
                             ui::OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperations(views::MenuItemView* sender) OVERRIDE;
  virtual views::MenuItemView* GetSiblingMenu(
      views::MenuItemView* menu,
      const gfx::Point& screen_point,
      views::MenuItemView::AnchorPosition* anchor,
      bool* has_mnemonics,
      views::MenuButton** button) OVERRIDE;
  virtual int GetMaxWidthForMenu(views::MenuItemView* view) OVERRIDE;

  // BookmarkModelObserver methods.
  virtual void BookmarkModelChanged() OVERRIDE;

 private:
  // BookmarkMenuController deletes itself as necessary.
  virtual ~BookmarkMenuController();

  scoped_ptr<views::MenuRunner> menu_runner_;

  scoped_ptr<BookmarkMenuDelegate> menu_delegate_;

  // The node we're showing the contents of.
  const BookmarkNode* node_;

  // Data for the drop.
  BookmarkNodeData drop_data_;

  // The observer, may be null.
  Observer* observer_;

  // Is the menu being shown for a drop?
  bool for_drop_;

  // The bookmark bar. This is only non-null if we're showing a menu item
  // for a folder on the bookmark bar and not for drop.
  BookmarkBarView* bookmark_bar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
