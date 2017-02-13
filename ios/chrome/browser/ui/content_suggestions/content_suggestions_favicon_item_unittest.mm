// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_internal_cell.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

ContentSuggestionsFaviconInternalCell* GetInnerCell(
    NSInteger cellIndex,
    ContentSuggestionsFaviconCell* outerCell) {
  UICollectionViewCell* innerCell = [outerCell.collectionView.dataSource
              collectionView:outerCell.collectionView
      cellForItemAtIndexPath:[NSIndexPath indexPathForItem:cellIndex
                                                 inSection:0]];

  EXPECT_EQ([ContentSuggestionsFaviconInternalCell class], [innerCell class]);
  return base::mac::ObjCCastStrict<ContentSuggestionsFaviconInternalCell>(
      innerCell);
}

#pragma mark - Tests

// Tests that configureCell: set all the fields of the cell.
TEST(ContentSuggestionsFaviconItemTest, CellIsConfigured) {
  id delegateMock = [OCMockObject
      mockForProtocol:@protocol(ContentSuggestionsFaviconCellDelegate)];
  ContentSuggestionsFaviconItem* item =
      [[ContentSuggestionsFaviconItem alloc] initWithType:0];
  item.delegate = delegateMock;

  ContentSuggestionsFaviconCell* cell = [[[item cellClass] alloc] init];
  ASSERT_EQ([ContentSuggestionsFaviconCell class], [cell class]);

  [item configureCell:cell];
  EXPECT_EQ(delegateMock, cell.delegate);
}

// Tests that the favicon added to the item are correctly passed to the inner
// collection view.
TEST(ContentSuggestionsFaviconItemTest, AddAFavicon) {
  ContentSuggestionsFaviconItem* item =
      [[ContentSuggestionsFaviconItem alloc] initWithType:0];
  UIImage* image1 = [[UIImage alloc] init];
  NSString* title1 = @"testTitle1";
  UIImage* image2 = [[UIImage alloc] init];
  NSString* title2 = @"testTitle2";
  [item addFavicon:image1 withTitle:title1];
  [item addFavicon:image2 withTitle:title2];

  ContentSuggestionsFaviconCell* cell = [[[item cellClass] alloc] init];

  [item configureCell:cell];

  ContentSuggestionsFaviconInternalCell* faviconInternalCell1 =
      GetInnerCell(0, cell);
  EXPECT_EQ(image1, faviconInternalCell1.faviconView.image);
  EXPECT_TRUE([title1 isEqualToString:faviconInternalCell1.titleLabel.text]);

  ContentSuggestionsFaviconInternalCell* faviconInternalCell2 =
      GetInnerCell(1, cell);
  EXPECT_EQ(image2, faviconInternalCell2.faviconView.image);
  EXPECT_TRUE([title2 isEqualToString:faviconInternalCell2.titleLabel.text]);
}

// Tests that adding nil favicon does nothing.
TEST(ContentSuggestionsFaviconItemTest, AddNilFavicon) {
  ContentSuggestionsFaviconItem* item =
      [[ContentSuggestionsFaviconItem alloc] initWithType:0];
  UIImage* image = [[UIImage alloc] init];
  NSString* title = @"testTitle1";
  UIImage* nilImage = nil;
  NSString* nilTitle = nil;
  [item addFavicon:nilImage withTitle:title];
  [item addFavicon:image withTitle:nilTitle];

  ContentSuggestionsFaviconCell* cell = [[[item cellClass] alloc] init];

  [item configureCell:cell];

  EXPECT_EQ(0,
            [cell.collectionView.dataSource collectionView:cell.collectionView
                                    numberOfItemsInSection:0]);
}

}  // namespace
