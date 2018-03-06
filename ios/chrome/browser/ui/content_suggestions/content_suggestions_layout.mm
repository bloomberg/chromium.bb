// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout.h"

#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsLayout

- (CGSize)collectionViewContentSize {
  CGFloat collectionViewHeight = self.collectionView.bounds.size.height;
  CGFloat headerHeight = [self firstHeaderHeight];

  // The minimum height for the collection view content should be the height of
  // the header plus the height of the collection view minus the height of the
  // NTP bottom bar. This allows the Most Visited cells to be scrolled up to the
  // top of the screen.
  CGFloat minimumHeight = collectionViewHeight + headerHeight -
                          ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsIPadIdiom())
    minimumHeight -= ntp_header::ToolbarHeight();

  CGSize contentSize = [super collectionViewContentSize];
  if (contentSize.height < minimumHeight) {
    contentSize.height = minimumHeight;
  }
  return contentSize;
}

#pragma mark - Private.

// Returns the height of the header of the first section.
- (CGFloat)firstHeaderHeight {
  id<UICollectionViewDelegateFlowLayout> delegate =
      static_cast<id<UICollectionViewDelegateFlowLayout>>(
          self.collectionView.delegate);
  return [delegate collectionView:self.collectionView
                                      layout:self
             referenceSizeForHeaderInSection:0]
      .height;
}

@end
