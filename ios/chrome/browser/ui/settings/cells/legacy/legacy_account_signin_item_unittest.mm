// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_account_signin_item.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests that the UIImage and UILabels are set properly after a call to
// |configureCell:|.

using LegacyAccountSignInItemTest = PlatformTest;

TEST_F(LegacyAccountSignInItemTest, ImageView) {
  LegacyAccountSignInItem* item =
      [[LegacyAccountSignInItem alloc] initWithType:0];
  UIImage* image = [[UIImage alloc] init];

  item.image = image;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyAccountSignInCell class]]);

  LegacyAccountSignInCell* signInCell = cell;
  EXPECT_FALSE(signInCell.imageView.image);

  [item configureCell:cell];
  EXPECT_NSEQ(image, signInCell.imageView.image);
}
