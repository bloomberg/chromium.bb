// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_SIGNIN_PROMO_CELL_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_SIGNIN_PROMO_CELL_H_

#import <UIKit/UIKit.h>

@class SigninPromoView;

typedef void (^CloseButtonCallback)(void);

// Sign-in promo cell based on SigninPromoView. This cell invites the user to
// login without typing their password.
@interface BookmarkSigninPromoCell : UICollectionViewCell

// Identifier for -[UICollectionView registerClass:forCellWithReuseIdentifier:].
+ (NSString*)reuseIdentifier;

@property(nonatomic, readonly) SigninPromoView* signinPromoView;
// Called when the user taps on the close button. If not set, the close button
// is hidden.
@property(nonatomic, copy) CloseButtonCallback closeButtonAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_SIGNIN_PROMO_CELL_H_
