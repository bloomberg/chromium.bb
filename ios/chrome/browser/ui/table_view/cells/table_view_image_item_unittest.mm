// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using TableViewImageItemTest = PlatformTest;
}

// Tests that the UILabel is set properly after a call to
// |configureCell:| and the image and chevron are visible.
TEST_F(TableViewImageItemTest, ItemProperties) {
  NSString* text = @"Cell text";

  TableViewImageItem* item = [[TableViewImageItem alloc] initWithType:0];
  item.title = text;
  item.image = [[UIImage alloc] init];

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewImageCell class]]);

  TableViewImageCell* imageCell =
      base::mac::ObjCCastStrict<TableViewImageCell>(cell);
  EXPECT_FALSE(imageCell.textLabel.text);
  EXPECT_FALSE(imageCell.imageView.image);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, imageCell.titleLabel.text);
  EXPECT_FALSE(imageCell.imageView.isHidden);
  EXPECT_FALSE(imageCell.chevronImageView.isHidden);
}

// Tests that the imageView is not visible if no image is set.
TEST_F(TableViewImageItemTest, ItemImageViewHidden) {
  NSString* text = @"Cell text";

  TableViewImageItem* item = [[TableViewImageItem alloc] initWithType:0];
  item.title = text;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewImageCell class]]);

  TableViewImageCell* imageCell =
      base::mac::ObjCCastStrict<TableViewImageCell>(cell);
  EXPECT_FALSE(item.image);
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_FALSE(item.image);
  EXPECT_TRUE(imageCell.imageView.isHidden);
}

// Tests that the chevron is not visible if hideChevron is YES.
TEST_F(TableViewImageItemTest, ItemChevronViewHidden) {
  NSString* text = @"Cell text";

  TableViewImageItem* item = [[TableViewImageItem alloc] initWithType:0];
  item.title = text;
  item.hideChevron = YES;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewImageCell class]]);

  TableViewImageCell* imageCell =
      base::mac::ObjCCastStrict<TableViewImageCell>(cell);
  EXPECT_FALSE(imageCell.chevronImageView.isHidden);
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_TRUE(imageCell.chevronImageView.isHidden);
}
