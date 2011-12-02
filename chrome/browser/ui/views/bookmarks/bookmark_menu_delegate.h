// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_
#pragma once

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "ui/views/controls/menu/menu_delegate.h"

class BookmarkNode;
class PageNavigator;
class Profile;

namespace ui {
class OSExchangeData;
}

namespace views {
class MenuItemView;
class Widget;
}

// BookmarkMenuDelegate acts as the (informal) views::MenuDelegate for showing
// bookmarks in a MenuItemView. BookmarkMenuDelegate informally implements
// MenuDelegate as its assumed another class is going to forward the appropriate
// methods to this class. Doing so allows this class to be used for both menus
// on the bookmark bar and the bookmarks in the wrench menu.
class BookmarkMenuDelegate : public BaseBookmarkModelObserver,
                             public BookmarkContextMenuObserver {
 public:
  enum ShowOptions {
    // Indicates a menu should be added containing the permanent folders (other
    // than then bookmark bar folder). This only makes sense when showing the
    // contents of the bookmark bar folder.
    SHOW_PERMANENT_FOLDERS,

    // Don't show any additional folders.
    HIDE_PERMANENT_FOLDERS
  };

  BookmarkMenuDelegate(Profile* profile,
                       PageNavigator* navigator,
                       views::Widget* parent,
                       int first_menu_id);
  virtual ~BookmarkMenuDelegate();

  // Creates the menus from the model.
  void Init(views::MenuDelegate* real_delegate,
            views::MenuItemView* parent,
            const BookmarkNode* node,
            int start_child_index,
            ShowOptions show_options,
            bookmark_utils::BookmarkLaunchLocation);

  // Sets the PageNavigator.
  void SetPageNavigator(PageNavigator* navigator);

  // Returns the id given to the next menu.
  int next_menu_id() const { return next_menu_id_; }

  // Makes the menu for |node| the active menu. |start_index| is the index of
  // the first child of |node| to show in the menu.
  void SetActiveMenu(const BookmarkNode* node, int start_index);

  // Returns the menu.
  views::MenuItemView* menu() const { return menu_; }

  // Returns the context menu, or NULL if the context menu isn't showing.
  views::MenuItemView* context_menu() const {
    return context_menu_.get() ? context_menu_->menu() : NULL;
  }

  Profile* profile() { return profile_; }

  views::Widget* parent() { return parent_; }

  // Returns true if we're in the process of mutating the model. This happens
  // when the user deletes menu items using the context menu.
  bool is_mutating_model() const { return is_mutating_model_; }

  // MenuDelegate like methods (see class description for details).
  string16 GetTooltipText(int id, const gfx::Point& p) const;
  bool IsTriggerableEvent(views::MenuItemView* menu,
                          const views::MouseEvent& e);
  void ExecuteCommand(int id, int mouse_event_flags);
  bool GetDropFormats(
      views::MenuItemView* menu,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats);
  bool AreDropTypesRequired(views::MenuItemView* menu);
  bool CanDrop(views::MenuItemView* menu, const ui::OSExchangeData& data);
  int GetDropOperation(views::MenuItemView* item,
                       const views::DropTargetEvent& event,
                       views::MenuDelegate::DropPosition* position);
  int OnPerformDrop(views::MenuItemView* menu,
                    views::MenuDelegate::DropPosition position,
                    const views::DropTargetEvent& event);
  bool ShowContextMenu(views::MenuItemView* source,
                       int id,
                       const gfx::Point& p,
                       bool is_mouse_gesture);
  bool CanDrag(views::MenuItemView* menu);
  void WriteDragData(views::MenuItemView* sender, ui::OSExchangeData* data);
  int GetDragOperations(views::MenuItemView* sender);
  int GetMaxWidthForMenu(views::MenuItemView* menu);

  // BookmarkModelObserver methods.
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;

  // BookmarkContextMenu::Observer methods.
  virtual void WillRemoveBookmarks(
      const std::vector<const BookmarkNode*>& bookmarks) OVERRIDE;
  virtual void DidRemoveBookmarks() OVERRIDE;

 private:
  typedef std::map<int, const BookmarkNode*> MenuIDToNodeMap;
  typedef std::map<const BookmarkNode*, int> NodeToMenuIDMap;
  typedef std::map<const BookmarkNode*, views::MenuItemView*> NodeToMenuMap;

  // Creates a menu and adds it to node_to_menu_id_map_. This uses
  // BuildMenu to recursively populate the menu.
  views::MenuItemView* CreateMenu(const BookmarkNode* parent,
                                  int start_child_index,
                                  ShowOptions show_options);

  // Invokes BuildMenuForPermanentNode() for the permanent nodes (excluding
  // 'other bookmarks' folder).
  void BuildMenusForPermanentNodes(views::MenuItemView* menu,
                                   int* next_menu_id);

  // If |node| has children a new menu is created and added to |menu| to
  // represent it. If |node| is not empty and |added_separator| is false, a
  // separator is added before the new menu items and |added_separator| is set
  // to true.
  void BuildMenuForPermanentNode(const BookmarkNode* node,
                                 views::MenuItemView* menu,
                                 int* next_menu_id,
                                 bool* added_separator);

  // Creates an entry in menu for each child node of |parent| starting at
  // |start_child_index|.
  void BuildMenu(const BookmarkNode* parent,
                 int start_child_index,
                 views::MenuItemView* menu,
                 int* next_menu_id);

  // Returns the menu whose id is |id|.
  views::MenuItemView* GetMenuByID(int id);

  Profile* profile_;

  PageNavigator* page_navigator_;

  // Parent of menus.
  views::Widget* parent_;

  // Maps from menu id to BookmarkNode.
  MenuIDToNodeMap menu_id_to_node_map_;

  // Mapping from node to menu id. This only contains entries for nodes of type
  // URL.
  NodeToMenuIDMap node_to_menu_id_map_;

  // Current menu.
  views::MenuItemView* menu_;

  // Data for the drop.
  BookmarkNodeData drop_data_;

  // Used when a context menu is shown.
  scoped_ptr<BookmarkContextMenu> context_menu_;

  // Is the menu being shown for a drop?
  bool for_drop_;

  // If non-NULL this is the |parent| passed to Init and is NOT owned by us.
  views::MenuItemView* parent_menu_item_;

  // Maps from node to menu. This is used if a NULL parent is passed to Init().
  NodeToMenuMap node_to_menu_map_;

  // ID of the next menu item.
  int next_menu_id_;

  views::MenuDelegate* real_delegate_;

  // Is the model being changed?
  bool is_mutating_model_;

  // The location where this bookmark menu will be displayed (for UMA).
  bookmark_utils::BookmarkLaunchLocation location_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_
