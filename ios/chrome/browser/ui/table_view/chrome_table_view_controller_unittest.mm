// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Checks that key methods are called.
// TableViewItem can't easily be mocked via OCMock as one of the methods to
// mock returns a Class type.
@interface FakeTableViewItem : TableViewItem
@property(nonatomic, assign) BOOL configureCellCalled;
@end

@implementation FakeTableViewItem

@synthesize configureCellCalled = _configureCellCalled;

- (void)configureCell:(UITableViewCell*)cell {
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
};

using ChromeTableViewControllerTest = PlatformTest;

TEST_F(ChromeTableViewControllerTest, CellForItemAtIndexPath) {
  ChromeTableViewController* controller =
      [[ChromeTableViewController alloc] initWithStyle:UITableViewStylePlain];
  [controller loadModel];

  [[controller tableViewModel] addSectionWithIdentifier:SectionIdentifierFoo];
  FakeTableViewItem* someItem =
      [[FakeTableViewItem alloc] initWithType:ItemTypeFooBar];
  [[controller tableViewModel] addItem:someItem
               toSectionWithIdentifier:SectionIdentifierFoo];

  ASSERT_EQ(NO, [someItem configureCellCalled]);
  [controller tableView:[controller tableView]
      cellForRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]];
  EXPECT_EQ(YES, [someItem configureCellCalled]);
}

}  // namespace
