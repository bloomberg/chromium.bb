// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MENU_CELL_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MENU_CELL_H_

#import <UIKit/UIKit.h>

@class BookmarkMenuItem;
@class MDCInkView;

// A single cell of the bookmark menu.
@interface BookmarkMenuCell : UITableViewCell

// The view used to display the ink effect.
@property(nonatomic, retain) MDCInkView* inkView;

// Updates the UI of the cell to reflect the menu item.
- (void)updateWithBookmarkMenuItem:(BookmarkMenuItem*)item
                           primary:(BOOL)primary;
+ (NSString*)reuseIdentifier;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MENU_CELL_H_
