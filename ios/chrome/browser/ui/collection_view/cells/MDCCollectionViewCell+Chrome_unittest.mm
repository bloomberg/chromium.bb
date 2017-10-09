// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeCollectionViewCell : MDCCollectionViewCell
@end

@implementation FakeCollectionViewCell
@end

@interface FakeCollectionViewItem : CollectionViewItem
@property(nonatomic, assign) NSInteger configureCount;
@end
@implementation FakeCollectionViewItem
@synthesize configureCount = _configureCount;
- (void)configureCell:(MDCCollectionViewCell*)cell {
  [super configureCell:cell];
  EXPECT_TRUE([cell isMemberOfClass:[FakeCollectionViewCell class]]);
  self.configureCount++;
}
@end

namespace {

using MDCCollectionViewCellChrome = PlatformTest;

TEST_F(MDCCollectionViewCellChrome, PreferredHeightCallsConfigureCell) {
  FakeCollectionViewItem* item =
      [[FakeCollectionViewItem alloc] initWithType:0];
  item.cellClass = [FakeCollectionViewCell class];
  EXPECT_EQ(0, item.configureCount);

  [MDCCollectionViewCell cr_preferredHeightForWidth:0 forItem:item];

  EXPECT_EQ(1, item.configureCount);
}

TEST_F(MDCCollectionViewCellChrome, PreferredHeight) {
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:0];
  footerItem.text = @"This is a pretty lengthy sentence.";
  CGFloat (^heightForWidth)(CGFloat width) = ^(CGFloat width) {
    return [MDCCollectionViewCell cr_preferredHeightForWidth:width
                                                     forItem:footerItem];
  };
  EXPECT_NEAR(heightForWidth(300), 50, 5);
  EXPECT_NEAR(heightForWidth(100), 115, 5);
}

}  // namespace
