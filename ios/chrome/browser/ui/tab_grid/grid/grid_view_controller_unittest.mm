// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid/grid_view_controller.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test object that exposes the inner state for test verification.
@interface TestGridViewController : GridViewController
@property(nonatomic, readonly) NSMutableArray<GridItem*>* items;
@property(nonatomic, readonly) NSUInteger selectedIndex;
@property(nonatomic, readonly) UICollectionView* collectionView;
@end
@implementation TestGridViewController
@dynamic items;
@dynamic selectedIndex;
@dynamic collectionView;
@end

// Fake object that conforms to GridViewControllerDelegate.
@interface FakeGridViewControllerDelegate : NSObject<GridViewControllerDelegate>
@property(nonatomic, assign) NSUInteger itemCount;
@end
@implementation FakeGridViewControllerDelegate
@synthesize itemCount = _itemCount;
- (void)gridViewController:(GridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count {
  self.itemCount = count;
}
- (void)gridViewController:(GridViewController*)gridViewController
      didSelectItemAtIndex:(NSUInteger)index {
  // No-op for unittests. This is only called when a user taps on a cell, not
  // generically when selectedIndex is updated.
}
- (void)gridViewController:(GridViewController*)gridViewController
       didCloseItemAtIndex:(NSUInteger)index {
  // No-op for unittests. This is only called when a user taps to close a cell,
  // not generically when items are removed from the data source.
}
@end

class GridViewControllerTest : public PlatformTest {
 public:
  GridViewControllerTest() {
    view_controller_ = [[TestGridViewController alloc] init];
    [view_controller_
        populateItems:@[ [[GridItem alloc] init], [[GridItem alloc] init] ]
        selectedIndex:0];
    delegate_ = [[FakeGridViewControllerDelegate alloc] init];
    delegate_.itemCount = 2;
    view_controller_.delegate = delegate_;
  }

 protected:
  TestGridViewController* view_controller_;
  FakeGridViewControllerDelegate* delegate_;
};

// Tests that items are initialized and delegate is updated with a new
// itemCount.
TEST_F(GridViewControllerTest, InitializeItems) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  GridItem* item = [[GridItem alloc] init];
  item.identifier = @"NEW-ITEM";
  [view_controller_ populateItems:@[ item ] selectedIndex:0];
  EXPECT_NSEQ(@"NEW-ITEM", view_controller_.items[0].identifier);
  EXPECT_EQ(1U, view_controller_.items.count);
  EXPECT_EQ(0U, view_controller_.selectedIndex);
  EXPECT_EQ(1U, delegate_.itemCount);
}

// Tests that an item is inserted and delegate is updated with a new itemCount.
TEST_F(GridViewControllerTest, InsertItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ insertItem:[[GridItem alloc] init]
                       atIndex:0
                 selectedIndex:2];
  EXPECT_EQ(3U, view_controller_.items.count);
  EXPECT_EQ(2U, view_controller_.selectedIndex);
  EXPECT_EQ(3U, delegate_.itemCount);
}

// Tests that an item is removed and delegate is updated with a new itemCount.
TEST_F(GridViewControllerTest, RemoveItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ removeItemAtIndex:0 selectedIndex:1];
  EXPECT_EQ(1U, view_controller_.items.count);
  EXPECT_EQ(1U, view_controller_.selectedIndex);
  EXPECT_EQ(1U, delegate_.itemCount);
}

// Tests that an item is selected.
TEST_F(GridViewControllerTest, SelectItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ selectItemAtIndex:1];
  EXPECT_EQ(1U, view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that an item is replaced.
TEST_F(GridViewControllerTest, ReplaceItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  GridItem* item = [[GridItem alloc] init];
  item.identifier = @"NEW-ITEM";
  [view_controller_ replaceItemAtIndex:0 withItem:item];
  EXPECT_NSEQ(@"NEW-ITEM", view_controller_.items[0].identifier);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that an item is moved.
TEST_F(GridViewControllerTest, MoveItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  view_controller_.items[0].identifier = @"ITEM-0";
  [view_controller_ moveItemFromIndex:0 toIndex:1 selectedIndex:1];
  EXPECT_NSEQ(@"ITEM-0", view_controller_.items[1].identifier);
  EXPECT_EQ(1U, view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}
