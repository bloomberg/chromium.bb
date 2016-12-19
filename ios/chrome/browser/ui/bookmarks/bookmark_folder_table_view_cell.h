// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_TABLE_VIEW_CELL_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_TABLE_VIEW_CELL_H_

#import <UIKit/UIKit.h>

// Table view cell that appears in the folder picker.
@interface BookmarkFolderTableViewCell : UITableViewCell

// Whether the cell is displaying a checkmark.
@property(nonatomic, assign, getter=isChecked) BOOL checked;

// Whether the cell is enabled for interaction.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// Returns the reuse identifier for folder cells.
+ (NSString*)folderCellReuseIdentifier;

// Returns the reuse identifier for the folder creation cell.
+ (NSString*)folderCreationCellReuseIdentifier;

// Returns a cell to be used to display a given folder. It displays a folder
// icon. The indentation level can be used to signify the depth in a hierarchy.
// The icon and title are then indented according to the indentation level.
+ (instancetype)folderCell;

// Returns a cell to create a new folder. It displays a folder icon with a '+'
// sign in it. The title prompts the user to create a new folder.
+ (instancetype)folderCreationCell;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_TABLE_VIEW_CELL_H_
