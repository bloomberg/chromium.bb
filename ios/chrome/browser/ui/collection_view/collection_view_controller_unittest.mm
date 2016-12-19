// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

#include "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/test/base/scoped_block_swizzler.h"
#include "ios/chrome/test/block_cleanup_test.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#include "testing/gtest/include/gtest/gtest.h"

// Checks that key methods are called.
// CollectionViewItem can't easily be mocked via OCMock as one of the methods to
// mock returns a Class type.
@interface MockCollectionViewItem : CollectionViewItem
@property(nonatomic, assign) BOOL configureCellCalled;
@end

@implementation MockCollectionViewItem

@synthesize configureCellCalled = _configureCellCalled;

- (void)configureCell:(MDCCollectionViewCell*)cell {
  self.configureCellCalled = YES;
  [super configureCell:cell];
}

@end

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFoo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFooBar = kItemTypeEnumZero,
  ItemTypeFooBiz,
};

class CollectionViewControllerTest : public BlockCleanupTest {};

}  // namespace

TEST_F(CollectionViewControllerTest, InitDefaultStyle) {
  base::scoped_nsobject<CollectionViewController> controller(
      [[CollectionViewController alloc]
          initWithStyle:CollectionViewControllerStyleDefault]);
  EXPECT_EQ(nil, controller.get().appBar);
}

TEST_F(CollectionViewControllerTest, InitAppBarStyle) {
  base::scoped_nsobject<CollectionViewController> controller(
      [[CollectionViewController alloc]
          initWithStyle:CollectionViewControllerStyleAppBar]);
  EXPECT_NE(nil, controller.get().appBar);
}

TEST_F(CollectionViewControllerTest, CellForItemAtIndexPath) {
  base::scoped_nsobject<CollectionViewController> controller(
      [[CollectionViewController alloc]
          initWithStyle:CollectionViewControllerStyleDefault]);
  [controller loadModel];

  [[controller collectionViewModel]
      addSectionWithIdentifier:SectionIdentifierFoo];
  base::scoped_nsobject<MockCollectionViewItem> someItem(
      [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBar]);
  [[controller collectionViewModel] addItem:someItem
                    toSectionWithIdentifier:SectionIdentifierFoo];

  ASSERT_EQ(NO, [someItem configureCellCalled]);
  [controller collectionView:[controller collectionView]
      cellForItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]];
  EXPECT_EQ(YES, [someItem configureCellCalled]);
}

TEST_F(CollectionViewControllerTest, ReconfigureCells) {
  base::scoped_nsobject<CollectionViewController> controller(
      [[CollectionViewController alloc]
          initWithStyle:CollectionViewControllerStyleDefault]);
  [controller loadModel];

  CollectionViewModel* model = [controller collectionViewModel];
  [model addSectionWithIdentifier:SectionIdentifierFoo];

  base::scoped_nsobject<MockCollectionViewItem> firstReconfiguredItem(
      [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBar]);
  [model addItem:firstReconfiguredItem
      toSectionWithIdentifier:SectionIdentifierFoo];

  base::scoped_nsobject<MockCollectionViewItem> secondReconfiguredItem(
      [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBiz]);
  [model addItem:secondReconfiguredItem
      toSectionWithIdentifier:SectionIdentifierFoo];

  base::scoped_nsobject<MockCollectionViewItem> firstNonReconfiguredItem(
      [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBiz]);
  [model addItem:firstNonReconfiguredItem
      toSectionWithIdentifier:SectionIdentifierFoo];

  base::scoped_nsobject<MockCollectionViewItem> thirdReconfiguredItem(
      [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBiz]);
  [model addItem:thirdReconfiguredItem
      toSectionWithIdentifier:SectionIdentifierFoo];

  base::scoped_nsobject<MockCollectionViewItem> secondNonReconfiguredItem(
      [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBiz]);
  [model addItem:secondNonReconfiguredItem
      toSectionWithIdentifier:SectionIdentifierFoo];

  // The collection view is not visible on screen, so it has not created any of
  // its cells.  Swizzle |cellsForItemAtIndexPath:| and inject an implementation
  // for testing that always returns a non-nil cell.
  base::scoped_nsobject<MDCCollectionViewCell> dummyCell(
      [[MDCCollectionViewCell alloc] init]);
  {
    ScopedBlockSwizzler swizzler([UICollectionView class],
                                 @selector(cellForItemAtIndexPath:),
                                 ^(id self) {
                                   return dummyCell.get();
                                 });

    NSArray* itemsToReconfigure = @[
      firstReconfiguredItem, secondReconfiguredItem, thirdReconfiguredItem
    ];
    [controller reconfigureCellsForItems:itemsToReconfigure
                 inSectionWithIdentifier:SectionIdentifierFoo];
  }

  EXPECT_TRUE([firstReconfiguredItem configureCellCalled]);
  EXPECT_TRUE([secondReconfiguredItem configureCellCalled]);
  EXPECT_TRUE([thirdReconfiguredItem configureCellCalled]);

  EXPECT_FALSE([firstNonReconfiguredItem configureCellCalled]);
  EXPECT_FALSE([secondNonReconfiguredItem configureCellCalled]);
}
