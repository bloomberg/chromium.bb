// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_tab_cell.h"

#import <UIKit/UIKit.h>

#include "base/test/ios/wait_util.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_button.h"
#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestTabCell : TabCollectionTabCell
@property(nonatomic) UILabel* titleLabel;
@end

@implementation TestTabCell
@dynamic titleLabel;
@end

class TabCollectionTabCellTest : public PlatformTest {
 public:
  TabCollectionTabCellTest() {
    cell_ = [[TestTabCell alloc] init];
    snapshotCache_ = OCMClassMock([SnapshotCache class]);
    item_ = [[TabCollectionItem alloc] init];
    item_.tabID = @"1234";
    item_.title = @"Title";
    snapshot_ = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(30, 40), [UIColor blueColor]);
  }
  ~TabCollectionTabCellTest() override {}

 protected:
  TestTabCell* cell_;
  id snapshotCache_;
  TabCollectionItem* item_;
  UIImage* snapshot_;
};

// Tests that -configureCell updates the title.
TEST_F(TabCollectionTabCellTest, TestConfigureTitle) {
  EXPECT_EQ(nil, cell_.titleLabel.text);
  [cell_ configureCell:item_ snapshotCache:snapshotCache_];
  EXPECT_NSEQ(@"Title", cell_.titleLabel.text);
}

// Tests that -configureCell: does not change the cell's snapshot if the
// snapshotCache does not contain the image.
TEST_F(TabCollectionTabCellTest, TestSnapshotMissing) {
  OCMStub([snapshotCache_ retrieveImageForSessionID:[OCMArg any]
                                           callback:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^callback)(UIImage* image);
        [invocation getArgument:&callback atIndex:3];
        callback(nil);
      });
  [cell_ configureCell:item_ snapshotCache:snapshotCache_];
  EXPECT_EQ(nil, cell_.snapshot);
}

// Tests that -configureCell: updates the cell's snapshot from the cache.
TEST_F(TabCollectionTabCellTest, TestSnapshotUpdated) {
  OCMStub([snapshotCache_ retrieveImageForSessionID:[OCMArg any]
                                           callback:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^callback)(UIImage* image);
        [invocation getArgument:&callback atIndex:3];
        callback(snapshot_);
      });
  [cell_ configureCell:item_ snapshotCache:snapshotCache_];
  EXPECT_TRUE(
      ui::test::uiimage_utils::UIImagesAreEqual(snapshot_, cell_.snapshot));
}

// Tests that asynchronous snapshot retrieval does not set the image after
// -prepareForReuse.
TEST_F(TabCollectionTabCellTest, TestPrepareForReuse) {
  OCMStub([snapshotCache_ retrieveImageForSessionID:[OCMArg any]
                                           callback:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^callback)(UIImage* image);
        [invocation getArgument:&callback atIndex:3];
        // -prepareForReuse is called before the callback.
        [cell_ prepareForReuse];
        callback(snapshot_);
      });
  [cell_ configureCell:item_ snapshotCache:snapshotCache_];
  EXPECT_EQ(nil, cell_.snapshot);
}
