// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_SIGNIN_PROMO_CELL_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_SIGNIN_PROMO_CELL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"

@class SigninPromoView;

// Sign-in promo cell based on SigninPromoView. This cell invites the user to
// login without typing their password.
@interface BookmarkTableSigninPromoCell : TableViewCell

// Identifier for -[UITableView registerClass:forCellWithReuseIdentifier:].
+ (NSString*)reuseIdentifier;

@property(nonatomic, readonly) SigninPromoView* signinPromoView;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_SIGNIN_PROMO_CELL_H_
