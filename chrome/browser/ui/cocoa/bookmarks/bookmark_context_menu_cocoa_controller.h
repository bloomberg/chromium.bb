// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_CONTEXT_MENU_COCOA_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_CONTEXT_MENU_COCOA_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

@class BookmarkBarController;
class BookmarkContextMenuController;
class BookmarkContextMenuDelegateBridge;
class BookmarkModel;
class BookmarkNode;
@class MenuController;

// A controller to manage bookmark bar context menus. One instance of this
// class exists per bookmark bar controller, used for all of its context menus
// (including those for items in bookmark bar folder drop downs).
@interface BookmarkContextMenuCocoaController : NSObject {
 @private
  // Bookmark bar controller, used for retrieving the Browser, Profile and
  // NSWindow when instantiating the BookmarkContextMenuController. Weak, owns
  // us.
  BookmarkBarController* bookmarkBarController_;

  // Helper for receiving C++ callbacks.
  scoped_ptr<BookmarkContextMenuDelegateBridge> bridge_;

  // The current |bookmarkNode_| for which |bookmarkContextMenuController_| and
  // |menuController_| are initialized. Weak, owned by the BookmarkModel.
  const BookmarkNode* bookmarkNode_;

  // The cross-platform BookmarkContextMenuController, containing the logic for
  // which items and corresponding actions exist in the menu.
  scoped_ptr<BookmarkContextMenuController> bookmarkContextMenuController_;

  // Controller responsible for creating a Cocoa NSMenu from the cross-platform
  // SimpleMenuModel owned by the |bookmarkContextMenuController_|.
  base::scoped_nsobject<MenuController> menuController_;
}

// Initializes the BookmarkContextMenuCocoaController for the given bookmark
// bar |controller|.
- (id)initWithBookmarkBarController:(BookmarkBarController*)controller;

// Returns an NSMenu customized for |node|. Works under the assumption that
// only one menu should ever be shown at a time, and thus caches the last
// returned menu and re-creates it if a menu for a different node is requested.
// Passing in a NULL |node| will return the menu for "empty" placeholder.
- (NSMenu*)menuForBookmarkNode:(const BookmarkNode*)node;

// Returns an NSMenu customized for the bookmark bar.
- (NSMenu*)menuForBookmarkBar;

// Closes the menu, if it's currently open.
- (void)cancelTracking;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_CONTEXT_MENU_COCOA_CONTROLLER_H_
