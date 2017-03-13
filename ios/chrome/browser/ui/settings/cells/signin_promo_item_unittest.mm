// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/signin_promo_item.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests that the profile image, the profile name and the profile email are set
// properly after a call to |configureCell:|.
TEST(SigninPromoItemTest, ConfigureCell) {
  SigninPromoItem* item = [[SigninPromoItem alloc] initWithType:0];
  UIImage* image = [[UIImage alloc] init];

  item.profileImage = image;
  item.profileName = @"John Doe";
  item.profileEmail = @"johndoe@gmail.com";

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[SigninPromoCell class]]);

  SigninPromoCell* signInCell = cell;
  EXPECT_FALSE(signInCell.imageView.image);

  [item configureCell:signInCell];

  EXPECT_NSEQ(image, signInCell.imageView.image);
  NSString* upperCaseProfileName = [item.profileName uppercaseString];
  NSRange profileNameRange = [signInCell.signinButton.titleLabel.text
      rangeOfString:upperCaseProfileName];
  EXPECT_NE(profileNameRange.length, 0u);
  NSRange profileEmailRange =
      [signInCell.notMeButton.titleLabel.text rangeOfString:item.profileEmail];
  EXPECT_NE(profileEmailRange.length, 0u);
}
