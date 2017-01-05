// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_collection_view_controller.h"

#import <Foundation/Foundation.h>

#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

@interface TestSettingsRootCollectionViewController
    : SettingsRootCollectionViewController
@property(nonatomic, readonly) NSMutableArray* items;
- (void)reset;
@end

@implementation TestSettingsRootCollectionViewController {
  base::scoped_nsobject<NSMutableArray> _items;
}

- (NSMutableArray*)items {
  if (!_items) {
    _items.reset([[NSMutableArray alloc] init]);
  }
  return _items.get();
}

- (void)reset {
  _items.reset();
}

- (void)loadModel {
  [super loadModel];
  if ([self.items count] > 0) {
    [self.collectionViewModel
        addSectionWithIdentifier:kSectionIdentifierEnumZero];
  }
  for (CollectionViewItem* item in self.items) {
    [self.collectionViewModel addItem:item
              toSectionWithIdentifier:kSectionIdentifierEnumZero];
  }
}

@end

namespace {

class SettingsRootCollectionViewControllerTest : public PlatformTest {
 public:
  SettingsRootCollectionViewControllerTest()
      : controller_([[TestSettingsRootCollectionViewController alloc]
            initWithStyle:CollectionViewControllerStyleDefault]) {
    item1_.reset([[CollectionViewItem alloc] initWithType:kItemTypeEnumZero]);
    item2_.reset(
        [[CollectionViewItem alloc] initWithType:kItemTypeEnumZero + 10]);
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    [controller() reset];
  }

  TestSettingsRootCollectionViewController* controller() { return controller_; }
  CollectionViewItem* item1() { return item1_; }
  CollectionViewItem* item2() { return item2_; }

  id getCollectionViewItem(int section, int item) {
    return [controller().collectionViewModel
        itemAtIndexPath:[NSIndexPath indexPathForItem:item inSection:section]];
  }

  int numberOfSections() {
    return [controller().collectionViewModel numberOfSections];
  }

  int numberOfItems(int section) {
    return [controller().collectionViewModel numberOfItemsInSection:section];
  }

  base::scoped_nsobject<TestSettingsRootCollectionViewController> controller_;
  base::scoped_nsobject<CollectionViewItem> item1_;
  base::scoped_nsobject<CollectionViewItem> item2_;
};

TEST_F(SettingsRootCollectionViewControllerTest, Empty) {
  [controller() loadModel];
  EXPECT_EQ(0, numberOfSections());
}

TEST_F(SettingsRootCollectionViewControllerTest, ModelContents) {
  [controller().items addObjectsFromArray:@[ item1(), item2() ]];
  [controller() loadModel];
  EXPECT_EQ(1, numberOfSections());
  EXPECT_EQ(2, numberOfItems(0));
  EXPECT_EQ(item1(), getCollectionViewItem(0, 0));
  EXPECT_EQ(item2(), getCollectionViewItem(0, 1));
}

}  // namespace
