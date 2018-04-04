// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout_handset.h"

#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsLayoutHandset

- (NSArray*)layoutAttributesForElementsInRect:(CGRect)rect {
  NSMutableArray* layoutAttributes =
      [[super layoutAttributesForElementsInRect:rect] mutableCopy];
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
  if (!fixedHeaderAttributes) {
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
    CGFloat topSafeArea = 0;
    if (@available(iOS 11, *)) {
      topSafeArea = self.collectionView.safeAreaInsets.top;
    } else {
      topSafeArea = StatusBarHeight();
    }
    CGFloat minY = headerHeight - ntp_header::kMinHeaderHeight - topSafeArea;
    if (contentOffset.y > minY)
      origin.y = contentOffset.y - minY;
    attributes.frame = {origin, attributes.frame.size};
  }
  return attributes;
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBound {
  return YES;
}

@end
