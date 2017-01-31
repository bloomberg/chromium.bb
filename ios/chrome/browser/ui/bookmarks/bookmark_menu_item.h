// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MENU_ITEM_H_

#import <UIKit/UIKit.h>

namespace bookmarks {
class BookmarkNode;

// The types get cached, which means that their values must not change.
//
// This enum values are store on disk (see BookmarkPositionCache class)
// and must not be changed. New values must be added at the end, and if
// a value is removed it should not be reused.
typedef enum {
  // A very thin divider.
  MenuItemDivider = 1,
  // A BookmarkNode* that is a folder.
  MenuItemFolder = 2,
  // Section header.
  MenuItemSectionHeader = 4,
  MenuItemLast = MenuItemSectionHeader
} MenuItemType;

BOOL NumberIsValidMenuItemType(int number);
}  // namespace bookmarks

// This model object represents a single row in the bookmark menu.
// This model should only be used from the main thread.
@interface BookmarkMenuItem : NSObject

// Returns the title to show in the menu.
- (NSString*)titleForMenu;
// Returns the title to show in the navigation bar.
- (NSString*)titleForNavigationBar;
// Returns the value to use as accessibility identifier.
- (NSString*)accessibilityIdentifier;
// Returns the image to show in the menu.
- (UIImage*)imagePrimary:(BOOL)primary;
// Returns the height of the corresponding row in the menu.
- (CGFloat)height;
// Whether the row can be selected.
- (BOOL)canBeSelected;
// Whether the view controller associated with this menu item supports editing.
- (BOOL)supportsEditing;
// Returns the menuitem located at the root ancestor of this item.
- (BookmarkMenuItem*)parentItem;

+ (BookmarkMenuItem*)dividerMenuItem;
+ (BookmarkMenuItem*)folderMenuItemForNode:(const bookmarks::BookmarkNode*)node
                              rootAncestor:
                                  (const bookmarks::BookmarkNode*)ancestor;
+ (BookmarkMenuItem*)sectionMenuItemWithTitle:(NSString*)title;

// |folder| is only valid if |type| == MenuItemFolder or MenuItemManaged.
@property(nonatomic, assign, readonly) const bookmarks::BookmarkNode* folder;
@property(nonatomic, assign, readonly)
    const bookmarks::BookmarkNode* rootAncestor;
@property(nonatomic, assign, readonly) bookmarks::MenuItemType type;
// |sectionTitle| is only valid if |type| == MenuItemSectionHeader.
@property(nonatomic, copy, readonly) NSString* sectionTitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MENU_ITEM_H_
