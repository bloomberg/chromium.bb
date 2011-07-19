// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_TREE_BROWSER_CELL_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_TREE_BROWSER_CELL_H_
#pragma once

#import <Cocoa/Cocoa.h>

class BookmarkNode;

// Provides a custom cell as used in the BookmarkEditor.xib's folder tree
// browser view.  This cell customization adds target and action support
// not provided by the NSBrowserCell as well as contextual information
// identifying the bookmark node being edited and the column matrix
// control in which is contained the cell.
@interface BookmarkTreeBrowserCell : NSBrowserCell {
 @private
  const BookmarkNode* bookmarkNode_;  // weak
  NSMatrix* matrix_;  // weak
  id target_;  // weak
  SEL action_;
}

@property(nonatomic, assign) NSMatrix* matrix;
@property(nonatomic, assign) id target;
@property(nonatomic, assign) SEL action;

- (const BookmarkNode*)bookmarkNode;
- (void)setBookmarkNode:(const BookmarkNode*)bookmarkNode;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_TREE_BROWSER_CELL_H_
