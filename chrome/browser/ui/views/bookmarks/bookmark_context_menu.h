// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
#pragma once

#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu_controller_views.h"
#include "views/controls/menu/menu_delegate.h"

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
      gfx::NativeWindow parent_window,
      Profile* profile,
      PageNavigator* page_navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection);
  virtual ~BookmarkContextMenu();

  // Shows the context menu at the specified point.
  void RunMenuAt(const gfx::Point& point);

  views::MenuItemView* menu() const { return menu_.get(); }

  void set_observer(BookmarkContextMenuObserver* observer) {
    observer_ = observer;
  }

  // Overridden from views::MenuDelegate:
  virtual void ExecuteCommand(int command_id);
  virtual bool IsItemChecked(int command_id) const;
  virtual bool IsCommandEnabled(int command_id) const;
  virtual bool ShouldCloseAllMenusOnExecute(int id);

  // Overridden from BookmarkContextMenuControllerViewsDelegate:
  virtual void CloseMenu();
  virtual void AddItemWithStringId(int command_id, int string_id);
  virtual void AddSeparator();
  virtual void AddCheckboxItem(int command_id, int string_id);
  virtual void WillRemoveBookmarks(
      const std::vector<const BookmarkNode*>& bookmarks);
  virtual void DidRemoveBookmarks();

 private:
  scoped_ptr<BookmarkContextMenuControllerViews> controller_;

  // The parent of dialog boxes opened from the context menu.
  gfx::NativeWindow parent_window_;

  // The menu itself.
  scoped_ptr<views::MenuItemView> menu_;

  BookmarkContextMenuObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
