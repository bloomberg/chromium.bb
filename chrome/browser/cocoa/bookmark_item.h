// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"

@class BookmarkManagerController;
class BookmarkNode;

// Cocoa object that stands in for a C++ BookmarkNode.
@interface BookmarkItem : NSObject {
 @private
  const BookmarkNode* node_;  // weak
  BookmarkManagerController* manager_;  // weak
  scoped_nsobject<NSImage> icon_;
}

- (id)initWithBookmarkNode:(const BookmarkNode*)node
                   manager:(BookmarkManagerController*)manager;

// The item's underlying C++ node (nil for fake items).
@property (readonly) const BookmarkNode* node;
// The item's title.
@property (copy) NSString* title;
// The item's URL as a plain string (nil for folders).
@property (copy) NSString* URLString;
// The item's parent in the real bookmark tree.
@property (readonly) BookmarkItem* parent;
// The user-visible path to the item (slash-delimited).
@property (readonly) NSString* folderPath;
// Fixed items (Bookmarks Bar, Other) cannot be renamed, moved or deleted.
@property (readonly) BOOL isFixed;
// The bookmark's favicon (a folder icon for folders.)
@property (readonly) NSImage* icon;

// The bookmark node's persistent ID.
@property (readonly) id persistentID;
// Finds a descendant with the given persistent ID.
- (BookmarkItem*)itemWithPersistentID:(id)persistentID;

// Is this item a folder/group?
@property (readonly) BOOL isFolder;
// The number of direct children of a folder.
@property (readonly) NSUInteger numberOfChildren;
// The child at a given zero-based index.
- (BookmarkItem*)childAtIndex:(NSUInteger)index;
// The index of a direct child, or NSNotFound.
- (NSUInteger)indexOfChild:(BookmarkItem*)child;
// Is the item an indirect descendant of the receiver?
- (BOOL)hasDescendant:(BookmarkItem*)item;

// The number of direct children that are themselves folders.
@property (readonly) NSUInteger numberOfChildFolders;
// The index'th child folder.
- (BookmarkItem*)childFolderAtIndex:(NSUInteger)index;
// The index, by counting child folders, of the given folder.
- (NSUInteger)indexOfChildFolder:(BookmarkItem*)child;

// Moves an existing item into this folder at the given index.
- (void)moveItem:(BookmarkItem*)item toIndex:(int)index;

// Adds a child bookmark. Receiver must be a folder.
- (BookmarkItem*) addBookmarkWithTitle:(NSString*)title
                                   URL:(NSString*)urlString
                               atIndex:(int)index;
// Adds a child folder. Receiver must be a folder.
- (BookmarkItem*) addFolderWithTitle:(NSString*)title
                             atIndex:(int)index;
// Removes a child bookmark; returns YES on success, NO on failure.
- (BOOL) removeChild:(BookmarkItem*)child;

// Open a bookmark in a tab.
- (BOOL)open;

// The default bookmark favicon.
+ (NSImage*)defaultIcon;
// The default folder favicon.
+ (NSImage*)folderIcon;
@end
