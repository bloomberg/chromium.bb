// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid_layout.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kInterTabSpacing = 20.0f;
}  // namespace

@interface GridLayout ()
@property(nonatomic, assign) CGFloat startingTabWidth;
@property(nonatomic, assign) CGFloat maxTabWidth;
@property(nonatomic, strong) NSArray<NSIndexPath*>* indexPathsOfDeletingItems;
@end

@implementation GridLayout
@synthesize startingTabWidth = _startingTabWidth;
@synthesize maxTabWidth = _maxTabWidth;
@synthesize indexPathsOfDeletingItems = _indexPathsOfDeletingItems;

- (instancetype)init {
  if (self = [super init]) {
    _startingTabWidth = 200.0f;
    _maxTabWidth = 250.0f;
  }
  return self;
}

#pragma mark - UICollectionViewLayout

// This is called whenever the layout is invalidated, including during rotation.
- (void)prepareLayout {
  [super prepareLayout];
  if (@available(iOS 11, *)) {
    [self updateLayoutGivenWidth:self.collectionView.safeAreaLayoutGuide
                                     .layoutFrame.size.width];
  } else {
    [self updateLayoutGivenWidth:self.collectionView.bounds.size.width];
  }
}

- (void)prepareForCollectionViewUpdates:
    (NSArray<UICollectionViewUpdateItem*>*)updateItems {
  NSMutableArray<NSIndexPath*>* deletingItems =
      [NSMutableArray arrayWithCapacity:updateItems.count];
  for (UICollectionViewUpdateItem* item in updateItems) {
    if (item.updateAction == UICollectionUpdateActionDelete) {
      [deletingItems addObject:item.indexPathBeforeUpdate];
    }
  }
  self.indexPathsOfDeletingItems = [deletingItems copy];
}

- (UICollectionViewLayoutAttributes*)
finalLayoutAttributesForDisappearingItemAtIndexPath:
    (NSIndexPath*)itemIndexPath {
  UICollectionViewLayoutAttributes* attributes =
      [super finalLayoutAttributesForDisappearingItemAtIndexPath:itemIndexPath];
  // Disappearing items that aren't being deleted just use the default
  // attributes.
  if (![self.indexPathsOfDeletingItems containsObject:itemIndexPath]) {
    return attributes;
  }
  // Cells being deleted fade out, are scaled down, and drop downwards slightly.
  attributes.alpha = 0.0;
  // Scaled down to 60%.
  CGAffineTransform transform =
      CGAffineTransformScale(attributes.transform, 0.6, 0.6);
  // Translated down (positive-y direction) by 50% of the cell cell size.
  transform =
      CGAffineTransformTranslate(transform, 0, attributes.size.height * 0.5);
  attributes.transform = transform;
  return attributes;
}

- (void)finalizeCollectionViewUpdates {
  self.indexPathsOfDeletingItems = @[];
}

#pragma mark - Private

// This sets the appropriate itemSize given the width of the collection view.
- (void)updateLayoutGivenWidth:(CGFloat)width {
  int columns = static_cast<int>(floor(width - kInterTabSpacing) /
                                 (self.startingTabWidth + kInterTabSpacing));
  CGFloat tabWidth = (width - kInterTabSpacing * (columns + 1)) / columns;
  while (columns < 2 || tabWidth > self.maxTabWidth) {
    columns++;
    tabWidth = (width - kInterTabSpacing * (columns + 1)) / columns;
  }
  self.itemSize = CGSizeMake(tabWidth, tabWidth);
  self.sectionInset = UIEdgeInsetsMake(20.0f, 20.0f, 20.0f, 20.0f);
  self.minimumLineSpacing = kInterTabSpacing;
  self.minimumInteritemSpacing = kInterTabSpacing;
}

@end
