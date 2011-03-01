// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
#pragma once

#include <map>
#include <set>

#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "ui/gfx/native_widget_types.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/menu_item_view.h"

class BookmarkBarView;
class BookmarkContextMenu;
class BookmarkNode;
class Browser;
class PageNavigator;
class Profile;

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class OSExchangeData;
}  // namespace ui

namespace views {
class MenuButton;
}  // namespace views

// BookmarkMenuController is responsible for showing a menu of bookmarks,
// each item in the menu represents a bookmark.
// BookmarkMenuController deletes itself as necessary, although the menu can
// be explicitly hidden by way of the Cancel method.
class BookmarkMenuController : public BaseBookmarkModelObserver,
                               public views::MenuDelegate,
                               public BookmarkContextMenuObserver {
 public:
  // The observer is notified prior to the menu being deleted.
  class Observer {
   public:
    virtual void BookmarkMenuDeleted(BookmarkMenuController* controller) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Creates a BookmarkMenuController showing the children of |node| starting
  // at index |start_child_index|.
  BookmarkMenuController(Browser* browser,
                         Profile* profile,
                         PageNavigator* page_navigator,
                         gfx::NativeWindow parent,
                         const BookmarkNode* node,
                         int start_child_index);

  void RunMenuAt(BookmarkBarView* bookmark_bar, bool for_drop);

  // Shows the menu.
  void RunMenuAt(views::MenuButton* button,
                 views::MenuItemView::AnchorPosition position,
                 bool for_drop);

  // Hides the menu.
  void Cancel();

  // Returns the node the menu is showing for.
  const BookmarkNode* node() const { return node_; }

  // Returns the menu.
  views::MenuItemView* menu() const { return menu_; }

  // Returns the context menu, or NULL if the context menu isn't showing.
  views::MenuItemView* context_menu() const {
    return context_menu_.get() ? context_menu_->menu() : NULL;
  }

  void set_observer(Observer* observer) { observer_ = observer; }

  // MenuDelegate methods.
  virtual std::wstring GetTooltipText(int id, const gfx::Point& p);
  virtual bool IsTriggerableEvent(const views::MouseEvent& e);
  virtual void ExecuteCommand(int id, int mouse_event_flags);
  virtual bool GetDropFormats(
      views::MenuItemView* menu,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats);
  virtual bool AreDropTypesRequired(views::MenuItemView* menu);
  virtual bool CanDrop(views::MenuItemView* menu,
                       const ui::OSExchangeData& data);
  virtual int GetDropOperation(views::MenuItemView* item,
                               const views::DropTargetEvent& event,
                               DropPosition* position);
  virtual int OnPerformDrop(views::MenuItemView* menu,
                            DropPosition position,
                            const views::DropTargetEvent& event);
  virtual bool ShowContextMenu(views::MenuItemView* source,
                               int id,
                               const gfx::Point& p,
                               bool is_mouse_gesture);
  virtual void DropMenuClosed(views::MenuItemView* menu);
  virtual bool CanDrag(views::MenuItemView* menu);
  virtual void WriteDragData(views::MenuItemView* sender,
                             ui::OSExchangeData* data);
  virtual int GetDragOperations(views::MenuItemView* sender);
  virtual views::MenuItemView* GetSiblingMenu(
      views::MenuItemView* menu,
      const gfx::Point& screen_point,
      views::MenuItemView::AnchorPosition* anchor,
      bool* has_mnemonics,
      views::MenuButton** button);
  virtual int GetMaxWidthForMenu();

  // BookmarkModelObserver methods.
  virtual void BookmarkModelChanged();
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node);

  // BookmarkContextMenu::Observer methods.
  virtual void WillRemoveBookmarks(
      const std::vector<const BookmarkNode*>& bookmarks);
  virtual void DidRemoveBookmarks();

 private:
  typedef std::map<const BookmarkNode*, int> NodeToMenuIDMap;

  // BookmarkMenuController deletes itself as necessary.
  virtual ~BookmarkMenuController();

  // Creates a menu and adds it to node_to_menu_id_map_. This uses
  // BuildMenu to recursively populate the menu.
  views::MenuItemView* CreateMenu(const BookmarkNode* parent,
                                  int start_child_index);

  // Creates an entry in menu for each child node of |parent| starting at
  // |start_child_index|.
  void BuildMenu(const BookmarkNode* parent,
                 int start_child_index,
                 views::MenuItemView* menu,
                 int* next_menu_id);

  // Returns the menu whose id is |id|.
  views::MenuItemView* GetMenuByID(int id);

  // Does the work of processing WillRemoveBookmarks. On exit the set of removed
  // menus is added to |removed_menus|. It's up to the caller to delete the
  // the menus added to |removed_menus|.
  void WillRemoveBookmarksImpl(
      const std::vector<const BookmarkNode*>& bookmarks,
      std::set<views::MenuItemView*>* removed_menus);

  Browser* browser_;

  Profile* profile_;

  PageNavigator* page_navigator_;

  // Parent of menus.
  gfx::NativeWindow parent_;

  // The node we're showing the contents of.
  const BookmarkNode* node_;

  // Maps from menu id to BookmarkNode.
  std::map<int, const BookmarkNode*> menu_id_to_node_map_;

  // Mapping from node to menu id. This only contains entries for nodes of type
  // URL.
  NodeToMenuIDMap node_to_menu_id_map_;

  // Current menu.
  views::MenuItemView* menu_;

  // Data for the drop.
  BookmarkNodeData drop_data_;

  // Used when a context menu is shown.
  scoped_ptr<BookmarkContextMenu> context_menu_;

  // The observer, may be null.
  Observer* observer_;

  // Is the menu being shown for a drop?
  bool for_drop_;

  // The bookmark bar. This is only non-null if we're showing a menu item
  // for a folder on the bookmark bar and not for drop.
  BookmarkBarView* bookmark_bar_;

  typedef std::map<const BookmarkNode*, views::MenuItemView*> NodeToMenuMap;
  NodeToMenuMap node_to_menu_map_;

  // ID of the next menu item.
  int next_menu_id_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
