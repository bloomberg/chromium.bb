// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SIGNIN_PROMO_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SIGNIN_PROMO_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@class MDCFlatButton;

// SigninPromoItem is an item that configures a SigninPromoCell cell.
@interface SigninPromoItem : CollectionViewItem

// Item image.
@property(nonatomic, copy) UIImage* profileImage;
@property(nonatomic, copy) NSString* profileName;
@property(nonatomic, copy) NSString* profileEmail;

@end

// Cell representation for SigninPromoItem. The cell displays an image, a title
// text label, a sign-in button and a "Not me" button. This is intended to be
// used to let the user sign-in without entering his/her password based on a
// profile already known in Chrome/Chromium. The user can also remove the
// suggested profile if (s)he is not related to the suggested profile, using the
// "Not me" button.
@interface SigninPromoCell : MDCCollectionViewCell

// Profile imageView.
@property(nonatomic, readonly, strong) UIImageView* imageView;
// Cell title.
@property(nonatomic, readonly, strong) UILabel* textLabel;
// Signin button.
@property(nonatomic, readonly, strong) MDCFlatButton* signinButton;
// Not-me button.
@property(nonatomic, readonly, strong) MDCFlatButton* notMeButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SIGNIN_PROMO_ITEM_H_
