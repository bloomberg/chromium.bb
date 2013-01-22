// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Controller (MVC) for the bookmark menu.
// All bookmark menu item commands get directed here.
// Unfortunately there is already a C++ class named BookmarkMenuController.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_COCOA_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_COCOA_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "ui/base/window_open_disposition.h"

class BookmarkNode;
class BookmarkMenuBridge;

@interface BookmarkMenuCocoaController : NSObject<NSMenuDelegate> {
 @private
  BookmarkMenuBridge* bridge_;  // weak; owns me
  NSMenu *menu_;
}

// The Bookmarks menu
@property(nonatomic, readonly) NSMenu* menu;

// Return an autoreleased string to be used as a menu title for the
// given bookmark node.
+ (NSString*)menuTitleForNode:(const BookmarkNode*)node;

// Make a relevant tooltip string for node.
+ (NSString*)tooltipForNode:(const BookmarkNode*)node;

- (id)initWithBridge:(BookmarkMenuBridge *)bridge
             andMenu:(NSMenu*)menu;

// Called by any Bookmark menu item.
// The menu item's tag is the bookmark ID.
- (IBAction)openBookmarkMenuItem:(id)sender;
- (IBAction)openAllBookmarks:(id)sender;
- (IBAction)openAllBookmarksNewWindow:(id)sender;
- (IBAction)openAllBookmarksIncognitoWindow:(id)sender;

@end  // BookmarkMenuCocoaController


@interface BookmarkMenuCocoaController (ExposedForUnitTests)
- (const BookmarkNode*)nodeForIdentifier:(int)identifier;
- (void)openURLForNode:(const BookmarkNode*)node;
- (void)openAll:(NSInteger)tag
    withDisposition:(WindowOpenDisposition)disposition;
@end  // BookmarkMenuCocoaController (ExposedForUnitTests)

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_COCOA_CONTROLLER_H_
