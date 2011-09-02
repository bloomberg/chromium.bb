// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_FOLDER_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_FOLDER_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"

@class BookmarkBarController;
@class BookmarkButton;
class BookmarkMenuBridge;
class BookmarkModel;

// A controller for the menus that are attached to the folder buttons on the
// bookmark bar.
@interface BookmarkBarFolderController : NSObject<BookmarkMenuDelegate> {
 @private
  // The button whose click opened us.
  scoped_nsobject<BookmarkButton> parentButton_;

  // The bookmark bar controller. Weak.
  BookmarkBarController* barController_;

  // The root menu for this node.
  scoped_nsobject<NSMenu> menu_;

  // The class that builds the menu.
  scoped_ptr<BookmarkMenuBridge> menuBridge_;
}

// Designated initializer.
- (id)initWithParentButton:(BookmarkButton*)button
             bookmarkModel:(BookmarkModel*)model
             barController:(BookmarkBarController*)barController;

// Return the parent button that owns the bookmark folder we represent.
- (BookmarkButton*)parentButton;

// Opens the menu. This will retain itself before it runs the menu and will
// release itself when the menu closes.
- (void)openMenu;

// Closes the menu.
- (void)closeMenu;

// For the "Off The Side" chevron menu, this sets the index in the bookmark_bar
// node at which the folder should start showing menu items. Forwarded to the
// bridge.
- (void)setOffTheSideNodeStartIndex:(size_t)index;

@end

@interface BookmarkBarFolderController (ExposedForTesting)
- (BookmarkMenuBridge*)menuBridge;
@end

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_FOLDER_CONTROLLER_H_
