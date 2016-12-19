// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMO_CELL_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMO_CELL_H_

#import <UIKit/UIKit.h>

@class BookmarkPromoCell;

@protocol BookmarkPromoCellDelegate

// Called when the SIGN IN button is tapped.
- (void)bookmarkPromoCellDidTapSignIn:(BookmarkPromoCell*)bookmarkPromoCell;

// Called when the NO THANKS button is tapped.
- (void)bookmarkPromoCellDidTapDismiss:(BookmarkPromoCell*)bookmarkPromoCell;

@end

@interface BookmarkPromoCell : UICollectionViewCell

+ (NSString*)reuseIdentifier;

@property(nonatomic, assign) id<BookmarkPromoCellDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMO_CELL_H_
