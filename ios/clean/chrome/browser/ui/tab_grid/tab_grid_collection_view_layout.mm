// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_collection_view_layout.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMinTabWidth = 200.0f;
const CGFloat kMaxTabWidth = 250.0f;
const CGFloat kInterTabSpacing = 20.0f;
const UIEdgeInsets kSectionInset = {20.0f, 20.0f, 20.0f, 20.0f};
}

@implementation TabGridCollectionViewLayout

#pragma mark - UICollectionViewLayout

// This is called whenever the layout is invalidated, including during rotation.
- (void)prepareLayout {
  [super prepareLayout];
  [self updateLayoutWithBounds:[[self collectionView] bounds].size];
}

#pragma mark - Private

// This sets the appropriate itemSize given the bounds of the collection view.
- (void)updateLayoutWithBounds:(CGSize)boundsSize {
  int columns = static_cast<int>(floor(boundsSize.width - kInterTabSpacing) /
                                 (kMinTabWidth + kInterTabSpacing));
  CGFloat tabWidth =
      (boundsSize.width - kInterTabSpacing * (columns + 1)) / columns;
  while (columns < 2 || tabWidth > kMaxTabWidth) {
    columns++;
    tabWidth = (boundsSize.width - kInterTabSpacing * (columns + 1)) / columns;
  }
  self.itemSize = CGSizeMake(tabWidth, tabWidth);
  self.sectionInset = kSectionInset;
  self.minimumLineSpacing = kInterTabSpacing;
  self.minimumInteritemSpacing = kInterTabSpacing;
}

@end
