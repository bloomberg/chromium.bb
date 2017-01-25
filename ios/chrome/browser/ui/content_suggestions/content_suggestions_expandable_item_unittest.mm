// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_expandable_item.h"

#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test subclass of the ContentSuggestionsExpandableCell.
@interface TestContentSuggestionsExpandableCell
    : ContentSuggestionsExpandableCell

@property(nonatomic) BOOL expandCalled;
@property(nonatomic) BOOL collapseCalled;

@end

@implementation TestContentSuggestionsExpandableCell

@synthesize expandCalled;
@synthesize collapseCalled;

- (void)expand {
  [super expand];
  self.expandCalled = YES;
}

- (void)collapse {
  [super collapse];
  self.collapseCalled = YES;
}

@end

namespace {

// Tests that configureCell: set all the fields of the cell.
TEST(SuggestionsExpandableItemTest, CellIsConfigured) {
  NSString* title = @"testTitle";
  NSString* subtitle = @"testSubtitle";
  UIImage* image = [[UIImage alloc] init];
  NSString* details = @"testDetails";
  id mockDelegate = [OCMockObject
      mockForProtocol:@protocol(ContentSuggestionsExpandableCellDelegate)];

  SuggestionsExpandableItem* item =
      [[SuggestionsExpandableItem alloc] initWithType:0
                                                title:title
                                             subtitle:subtitle
                                                image:image
                                           detailText:details];
  item.delegate = mockDelegate;
  ContentSuggestionsExpandableCell* cell = [[[item cellClass] alloc] init];
  EXPECT_EQ([ContentSuggestionsExpandableCell class], [cell class]);

  [item configureCell:cell];
  EXPECT_EQ(title, cell.titleLabel.text);
  EXPECT_EQ(subtitle, cell.subtitleLabel.text);
  EXPECT_EQ(image, cell.imageView.image);
  EXPECT_EQ(details, cell.detailLabel.text);
  EXPECT_EQ(mockDelegate, cell.delegate);
}

// Tests that if the expanded property of the item is YES, the |expand| method
// of the cell is called during configuration.
TEST(SuggestionsExpandableItemTest, CellIsExpanded) {
  SuggestionsExpandableItem* item =
      [[SuggestionsExpandableItem alloc] initWithType:0
                                                title:@"title"
                                             subtitle:@"subtitle"
                                                image:nil
                                           detailText:@"detail"];
  TestContentSuggestionsExpandableCell* cell =
      [[TestContentSuggestionsExpandableCell alloc] init];
  item.cellClass = [TestContentSuggestionsExpandableCell class];

  item.expanded = YES;
  [item configureCell:cell];
  ASSERT_TRUE(cell.expandCalled);
  ASSERT_FALSE(cell.collapseCalled);
}

// Tests that if the expanded property of the item is NO, the |collapse| method
// of the cell is called during configuration.
TEST(SuggestionsExpandableItemTest, CellIsCollapsed) {
  SuggestionsExpandableItem* item =
      [[SuggestionsExpandableItem alloc] initWithType:0
                                                title:@"title"
                                             subtitle:@"subtitle"
                                                image:nil
                                           detailText:@"detail"];
  TestContentSuggestionsExpandableCell* cell =
      [[TestContentSuggestionsExpandableCell alloc] init];
  item.cellClass = [TestContentSuggestionsExpandableCell class];

  item.expanded = NO;
  [item configureCell:cell];
  ASSERT_TRUE(cell.collapseCalled);
  ASSERT_FALSE(cell.expandCalled);
}

}  // namespace
