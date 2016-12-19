// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/most_visited_layout.h"

#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"

@implementation MostVisitedLayout

- (NSArray*)layoutAttributesForElementsInRect:(CGRect)rect {
  NSMutableArray* layoutAttributes = [
      [[super layoutAttributesForElementsInRect:rect] mutableCopy] autorelease];
  UICollectionViewLayoutAttributes* fixedHeaderAttributes = nil;
  NSIndexPath* fixedHeaderIndexPath =
      [NSIndexPath indexPathForItem:0 inSection:0];

  for (UICollectionViewLayoutAttributes* attributes in layoutAttributes) {
    if ([attributes.representedElementKind
            isEqualToString:UICollectionElementKindSectionHeader] &&
        attributes.indexPath.section == fixedHeaderIndexPath.section) {
      fixedHeaderAttributes = [self
          layoutAttributesForSupplementaryViewOfKind:
              UICollectionElementKindSectionHeader
                                         atIndexPath:fixedHeaderIndexPath];
      attributes.zIndex = fixedHeaderAttributes.zIndex;
      attributes.frame = fixedHeaderAttributes.frame;
    }
  }

  // The fixed header's attributes are not updated if the header's default frame
  // is far enough away from |rect|, which can occur when the NTP is scrolled
  // up.
  if (!fixedHeaderAttributes && !IsIPadIdiom()) {
    UICollectionViewLayoutAttributes* fixedHeaderAttributes =
        [self layoutAttributesForSupplementaryViewOfKind:
                  UICollectionElementKindSectionHeader
                                             atIndexPath:fixedHeaderIndexPath];
    [layoutAttributes addObject:fixedHeaderAttributes];
  }

  return layoutAttributes;
}

- (UICollectionViewLayoutAttributes*)
layoutAttributesForSupplementaryViewOfKind:(NSString*)kind
                               atIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewLayoutAttributes* attributes =
      [super layoutAttributesForSupplementaryViewOfKind:kind
                                            atIndexPath:indexPath];
  if ([kind isEqualToString:UICollectionElementKindSectionHeader] &&
      indexPath.section == 0) {
    UICollectionView* collectionView = self.collectionView;
    CGPoint contentOffset = collectionView.contentOffset;
    CGFloat headerHeight = CGRectGetHeight(attributes.frame);
    CGPoint origin = attributes.frame.origin;

    // Keep the header in front of all other views.
    attributes.zIndex = 1000;

    // Prevent the fake omnibox from scrolling up off of the screen.
    CGFloat minY = headerHeight - ntp_header::kMinHeaderHeight;
    if (contentOffset.y > minY)
      origin.y = contentOffset.y - minY;
    attributes.frame = {origin, attributes.frame.size};
  }
  return attributes;
}

- (UICollectionViewLayoutAttributes*)layoutAttributesForItemAtIndexPath:
    (NSIndexPath*)indexPath {
  UICollectionViewLayoutAttributes* currentItemAttributes =
      [super layoutAttributesForItemAtIndexPath:indexPath];

  return currentItemAttributes;
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBound {
  return YES;
}

- (CGSize)collectionViewContentSize {
  CGFloat collectionViewHeight = self.collectionView.bounds.size.height;
  CGFloat headerHeight = [self headerReferenceSize].height;

  // The minimum height for the collection view content should be the height of
  // the header plus the height of the collection view minus the height of the
  // NTP bottom bar. This allows the Most Visited cells to be scrolled up to the
  // top of the screen.
  CGFloat minimumHeight = collectionViewHeight + headerHeight -
                          ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsIPadIdiom())
    minimumHeight -= ntp_header::kToolbarHeight;

  CGSize contentSize = [super collectionViewContentSize];
  if (contentSize.height < minimumHeight) {
    contentSize.height = minimumHeight;
  }
  return contentSize;
}

@end
