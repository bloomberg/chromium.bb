// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// C++ class that connects a BookmarkNode (or the entire model) to a Cocoa class
// that manages an NSMenu.  Commonly this is for the main menu and that instance
// is owned by the AppController.  This is also used by the folder menus on the
// bookmark bar.
//
// In the main menu case, most Chromium Cocoa menu items are static from a nib
// (e.g. New Tab), but may be enabled/disabled under certain circumstances
// (e.g. Cut and Paste).  In addition, most Cocoa menu items have
// firstResponder: as a target.  Unusually, bookmark menu items are
// created dynamically.  They also have a target of
// BookmarkMenuCocoaController instead of firstResponder.
// See BookmarkMenuBridge::AddNodeToMenu()).

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_BRIDGE_H_
#pragma once

#include <map>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#import "chrome/browser/ui/cocoa/main_menu_item.h"

class BookmarkNode;
class Profile;
@class NSImage;
@class NSMenu;
@class NSMenuItem;
@class BookmarkMenuCocoaController;

class BookmarkMenuBridge : public BookmarkModelObserver,
                           public MainMenuItem {
 public:
  // Constructor for the main menu which lists all bookmarks.
  BookmarkMenuBridge(Profile* profile, NSMenu* menu);

  // Constructor for a submenu.
  BookmarkMenuBridge(const BookmarkNode* root_node,
                     Profile* profile,
                     NSMenu* menu);

  virtual ~BookmarkMenuBridge();

  // BookmarkModelObserver:
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;

  // MainMenuItem:
  virtual void ResetMenu() OVERRIDE;
  virtual void BuildMenu() OVERRIDE;

  // Rebuilds the main bookmark menu, if it has been marked invalid.
  void UpdateMenu(NSMenu* bookmark_menu);

  // Rebuilds a bookmark menu that's a submenu of another menu.
  void UpdateSubMenu(NSMenu* bookmark_menu);

  // If this bridge is managing a menu for the "Off the Side" chevron button,
  // this sets the index in the menu of the first node to display.
  void set_off_the_side_node_start_index(size_t index) {
    off_the_side_node_start_index_ = index;
    InvalidateMenu();
  }

  // I wish I had a "friend @class" construct.
  BookmarkModel* GetBookmarkModel();
  Profile* GetProfile();
  BookmarkMenuCocoaController* controller() { return controller_.get(); }

 protected:
  // Rebuilds the bookmark content of supplied menu.
  void UpdateMenuInternal(NSMenu* bookmark_menu, bool is_submenu);

  // Clear all bookmarks from the given bookmark menu.
  void ClearBookmarkMenu(NSMenu* menu);

  // Mark the bookmark menu as being invalid.
  void InvalidateMenu()  { menu_is_valid_ = false; }

  // Helper for adding the node as a submenu to the menu with the
  // given title.
  // If |add_extra_items| is true, also adds extra menu items at bottom of
  // menu, such as "Open All Bookmarks".
  void AddNodeAsSubmenu(NSMenu* menu,
                        const BookmarkNode* node,
                        NSString* title,
                        bool add_extra_items);

  // Helper for recursively adding items to our bookmark menu.
  // All children of |node| will be added to |menu|.
  // If |add_extra_items| is true, also adds extra menu items at bottom of
  // menu, such as "Open All Bookmarks".
  // TODO(jrg): add a counter to enforce maximum nodes added
  void AddNodeToMenu(const BookmarkNode* node,
                     NSMenu* menu,
                     bool add_extra_items);

  // Helper for adding an item to our bookmark menu. An item which has a
  // localized title specified by |message_id| will be added to |menu|.
  // The item is also bound to |node| by tag. |command_id| selects the action.
  void AddItemToMenu(int command_id,
                     int message_id,
                     const BookmarkNode* node,
                     NSMenu* menu,
                     bool enabled);

  // This configures an NSMenuItem with all the data from a BookmarkNode. This
  // is used to update existing menu items, as well as to configure newly
  // created ones, like in AddNodeToMenu().
  // |set_title| is optional since it is only needed when we get a
  // node changed notification.  On initial build of the menu we set
  // the title as part of alloc/init.
  void ConfigureMenuItem(const BookmarkNode* node, NSMenuItem* item,
                         bool set_title);

  // Returns the NSMenuItem for a given BookmarkNode.
  NSMenuItem* MenuItemForNode(const BookmarkNode* node);

  // Return the Bookmark menu.
  virtual NSMenu* BookmarkMenu();

  // Start watching the bookmarks for changes.
  void ObserveBookmarkModel();

 private:
  friend class BookmarkMenuBridgeTest;

  // Performs the actual work for AddNodeToMenu(), keeping count of the
  // recursion depth.
  void AddNodeToMenuRecursive(const BookmarkNode* node,
                              NSMenu* menu,
                              bool add_extra_items,
                              int recursion_depth);

  // True iff the menu is up-to-date with the actual BookmarkModel.
  bool menu_is_valid_;

  // The root node of the menu.
  const BookmarkNode* root_node_;

  // Index from which to start adding children from the model.
  size_t off_the_side_node_start_index_;

  Profile* profile_;  // Weak.
  scoped_nsobject<BookmarkMenuCocoaController> controller_;

  // The folder image so we can use one copy for all.
  scoped_nsobject<NSImage> folder_image_;

  // In order to appropriately update items in the bookmark menu, without
  // forcing a rebuild, map the model's nodes to menu items.
  std::map<const BookmarkNode*, NSMenuItem*> bookmark_nodes_;
};

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_BRIDGE_H_
