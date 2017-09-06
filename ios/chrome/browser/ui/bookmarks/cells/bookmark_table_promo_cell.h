// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_PROMO_CELL_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_PROMO_CELL_H_

#import <UIKit/UIKit.h>

@class BookmarkTablePromoCell;

@protocol BookmarkTablePromoCellDelegate

// Called when the SIGN IN button is tapped.
- (void)bookmarkTablePromoCellDidTapSignIn:
    (BookmarkTablePromoCell*)bookmarkPromoCell;

// Called when the NO THANKS button is tapped.
- (void)bookmarkTablePromoCellDidTapDismiss:
    (BookmarkTablePromoCell*)bookmarkPromoCell;

@end

@interface BookmarkTablePromoCell : UITableViewCell

// Identifier for -[UITableView registerClass:forCellWithReuseIdentifier:].
+ (NSString*)reuseIdentifier;

@property(nonatomic, weak) id<BookmarkTablePromoCellDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_PROMO_CELL_H_
