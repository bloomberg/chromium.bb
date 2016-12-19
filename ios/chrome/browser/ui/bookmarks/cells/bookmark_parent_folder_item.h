// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_PARENT_FOLDER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_PARENT_FOLDER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Item to display the name of the parent folder of a bookmark node.
@interface BookmarkParentFolderItem : CollectionViewItem

// The title of the bookmark folder it represents.
@property(nonatomic, copy) NSString* title;

@end

// Cell class associated to BookmarkParentFolderItem.
@interface BookmarkParentFolderCell : MDCCollectionViewCell

// Label that displays the item's title.
@property(nonatomic, readonly, strong) UILabel* parentFolderNameLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_PARENT_FOLDER_ITEM_H_
