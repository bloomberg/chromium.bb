// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_CELL_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_CELL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Cell to display bookmark folders and URLs.
// |---------------------------------------------|
// |                                             |
// |[Favicon] [title]                         [>]|
// |                                             |
// |---------------------------------------------|
//
@interface BookmarkTableCell : UITableViewCell

- (instancetype)initWithNode:(const bookmarks::BookmarkNode*)node
             reuseIdentifier:(NSString*)bookmarkCellIdentifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns the preferred image size for favicons.
+ (CGFloat)preferredImageSize;

// Sets the favicon image.
- (void)setImage:(UIImage*)image;

// Sets placeholder text, when favicon is missing.
- (void)setPlaceholderText:(NSString*)text
                 textColor:(UIColor*)textColor
           backgroundColor:(UIColor*)backgroundColor;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_CELL_H_
