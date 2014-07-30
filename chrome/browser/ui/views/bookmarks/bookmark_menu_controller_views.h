// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_

#include <set>

#include "base/compiler_specific.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "ui/views/controls/menu/menu_delegate.h"

class BookmarkBarView;
class BookmarkMenuControllerObserver;
class BookmarkMenuDelegate;
class BookmarkNode;
class Browser;

namespace content {
class PageNavigator;
}

namespace ui {
class OSExchangeData;
}

namespace views {
class MenuButton;
class MenuItemView;
class MenuRunner;
class Widget;
}

// BookmarkMenuController is responsible for showing a menu of bookmarks,
// each item in the menu represents a bookmark.
// BookmarkMenuController deletes itself as necessary, although the menu can
// be explicitly hidden by way of the Cancel method.
class BookmarkMenuController : public BaseBookmarkModelObserver,
                               public views::MenuDelegate {
 public:
  // Creates a BookmarkMenuController showing the children of |node| starting
  // at |start_child_index|.
  BookmarkMenuController(Browser* browser,
                         content::PageNavigator* page_navigator,
                         views::Widget* parent,
                         const BookmarkNode* node,
                         int start_child_index,
                         bool for_drop);

  void RunMenuAt(BookmarkBarView* bookmark_bar);

  void clear_bookmark_bar() {
    bookmark_bar_ = NULL;
  }

  // Hides the menu.
  void Cancel();

  // Returns the node the menu is showing for.
  const BookmarkNode* node() const { return node_; }

  // Returns the menu.
  views::MenuItemView* menu() const;

  // Returns the context menu, or NULL if the context menu isn't showing.
  views::MenuItemView* context_menu() const;

  // Sets the page navigator.
  void SetPageNavigator(content::PageNavigator* navigator);

  void set_observer(BookmarkMenuControllerObserver* observer) {
    observer_ = observer;
  }

  // views::MenuDelegate:
  virtual base::string16 GetTooltipText(int id,
                                        const gfx::Point& p) const OVERRIDE;
  virtual bool IsTriggerableEvent(views::MenuItemView* view,
                                  const ui::Event& e) OVERRIDE;
  virtual void ExecuteCommand(int id, int mouse_event_flags) OVERRIDE;
  virtual bool ShouldExecuteCommandWithoutClosingMenu(
      int id,
      const ui::Event& e) OVERRIDE;
  virtual bool GetDropFormats(
      views::MenuItemView* menu,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool AreDropTypesRequired(views::MenuItemView* menu) OVERRIDE;
  virtual bool CanDrop(views::MenuItemView* menu,
                       const ui::OSExchangeData& data) OVERRIDE;
  virtual int GetDropOperation(views::MenuItemView* item,
                               const ui::DropTargetEvent& event,
                               DropPosition* position) OVERRIDE;
  virtual int OnPerformDrop(views::MenuItemView* menu,
                            DropPosition position,
                            const ui::DropTargetEvent& event) OVERRIDE;
  virtual bool ShowContextMenu(views::MenuItemView* source,
                               int id,
                               const gfx::Point& p,
                               ui::MenuSourceType source_type) OVERRIDE;
  virtual void DropMenuClosed(views::MenuItemView* menu) OVERRIDE;
  virtual bool CanDrag(views::MenuItemView* menu) OVERRIDE;
  virtual void WriteDragData(views::MenuItemView* sender,
                             ui::OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperations(views::MenuItemView* sender) OVERRIDE;
  virtual views::MenuItemView* GetSiblingMenu(
      views::MenuItemView* menu,
      const gfx::Point& screen_point,
      views::MenuAnchorPosition* anchor,
      bool* has_mnemonics,
      views::MenuButton** button) OVERRIDE;
  virtual int GetMaxWidthForMenu(views::MenuItemView* view) OVERRIDE;

  // BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;

 private:
  // BookmarkMenuController deletes itself as necessary.
  virtual ~BookmarkMenuController();

  scoped_ptr<views::MenuRunner> menu_runner_;

  scoped_ptr<BookmarkMenuDelegate> menu_delegate_;

  // The node we're showing the contents of.
  const BookmarkNode* node_;

  // Data for the drop.
  bookmarks::BookmarkNodeData drop_data_;

  // The observer, may be null.
  BookmarkMenuControllerObserver* observer_;

  // Is the menu being shown for a drop?
  bool for_drop_;

  // The bookmark bar. This is only non-null if we're showing a menu item for a
  // folder on the bookmark bar and not for drop, or if the BookmarkBarView has
  // been destroyed before the menu.
  BookmarkBarView* bookmark_bar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
