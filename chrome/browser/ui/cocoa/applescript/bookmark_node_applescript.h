// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_NODE_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_NODE_APPLESCRIPT_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/applescript/element_applescript.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

// Contains all the elements that are common to both a bookmark folder and
// bookmark item.
@interface BookmarkNodeAppleScript : ElementAppleScript {
 @protected
  const bookmarks::BookmarkNode* _bookmarkNode;  // weak.
  // Contains the temporary title when a scripter creates a new folder/item with
  // title specified like
  // |make new bookmark folder with properties {title:"foo"}|.
  NSString* _tempTitle;
}

// Does not actually create a folder/item but just sets its ID, the folder is
// created in insertInBookmarksFolder: in the corresponding bookmarks folder.
- (id)init;

// Does not make a folder/item but instead uses an existing one.
- (id)initWithBookmarkNode:(const bookmarks::BookmarkNode*)aBookmarkNode;

// Assigns a node, sets its unique ID and also copies temporary values.
- (void)setBookmarkNode:(const bookmarks::BookmarkNode*)aBookmarkNode;

// Get and Set title.
- (NSString*)title;
- (void)setTitle:(NSString*)aTitle;

// Returns the index with respect to its parent bookmark folder.
- (NSNumber*)index;

// Returns the bookmark model of the browser, returns NULL if there is an error.
- (bookmarks::BookmarkModel*)bookmarkModel;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_NODE_APPLESCRIPT_H_
