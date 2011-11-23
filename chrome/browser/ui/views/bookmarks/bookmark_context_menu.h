// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu_controller_views.h"
#include "views/controls/menu/menu_delegate.h"

namespace views {
class MenuRunner;
class Widget;
}

// Observer for the BookmarkContextMenu.
class BookmarkContextMenuObserver {
 public:
  // Invoked before the specified items are removed from the bookmark model.
  virtual void WillRemoveBookmarks(
      const std::vector<const BookmarkNode*>& bookmarks) = 0;

  // Invoked after the items have been removed from the model.
  virtual void DidRemoveBookmarks() = 0;

 protected:
  virtual ~BookmarkContextMenuObserver() {}
};

class BookmarkContextMenu : public BookmarkContextMenuControllerViewsDelegate,
                            public views::MenuDelegate {
 public:
  BookmarkContextMenu(
      views::Widget* parent_widget,
      Profile* profile,
      PageNavigator* page_navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection,
      bool close_on_remove);
  virtual ~BookmarkContextMenu();

  // Shows the context menu at the specified point.
  void RunMenuAt(const gfx::Point& point);

  views::MenuItemView* menu() const { return menu_; }

  void set_observer(BookmarkContextMenuObserver* observer) {
    observer_ = observer;
  }

  // Sets the PageNavigator.
  void SetPageNavigator(PageNavigator* navigator);

  // Overridden from views::MenuDelegate:
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual bool IsItemChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE;
  virtual bool ShouldCloseAllMenusOnExecute(int id) OVERRIDE;

  // Overridden from BookmarkContextMenuControllerViewsDelegate:
  virtual void CloseMenu() OVERRIDE;
  virtual void AddItemWithStringId(int command_id, int string_id) OVERRIDE;
  virtual void AddSeparator() OVERRIDE;
  virtual void AddCheckboxItem(int command_id, int string_id) OVERRIDE;
  virtual void WillRemoveBookmarks(
      const std::vector<const BookmarkNode*>& bookmarks) OVERRIDE;
  virtual void DidRemoveBookmarks() OVERRIDE;

 private:
  scoped_ptr<BookmarkContextMenuControllerViews> controller_;

  // The parent of dialog boxes opened from the context menu.
  views::Widget* parent_widget_;

  // The menu itself. This is owned by |menu_runner_|.
  views::MenuItemView* menu_;

  // Responsible for running the menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The node we're showing the menu for.
  const BookmarkNode* parent_node_;

  BookmarkContextMenuObserver* observer_;

  // Should the menu close when a node is removed.
  bool close_on_remove_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
