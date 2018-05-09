// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_FOLDER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_FOLDER_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

typedef NS_ENUM(NSInteger, BookmarkFolderStyle) {
  BookmarkFolderStyleFolderEntry,
  BookmarkFolderStyleNewFolder,
};

// BookmarkFolderItem provides data for a table view row that displays a
// single bookmark folder.
@interface BookmarkFolderItem : TableViewItem

// The Item's designated initializer. If |style| is
// BookmarkFolderStyle then all other property values will be
// ignored.
- (instancetype)initWithType:(NSInteger)type
                       style:(BookmarkFolderStyle)style
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

// The item's title.
@property(nonatomic, copy) NSString* title;
// YES if the FolderItem is the current folder.
@property(nonatomic, assign, getter=isCurrentFolder) BOOL currentFolder;
// The item's left indentation level.
@property(nonatomic, assign) NSInteger indentationLevel;

@end

// TableViewCell that displays BookmarkFolderItem data.
@interface LegacyTableViewBookmarkFolderCell : UITableViewCell

// Whether the cell is displaying a checkmark.
@property(nonatomic, assign, getter=isChecked) BOOL checked;

// Whether the cell is enabled for interaction.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_FOLDER_ITEM_H_
