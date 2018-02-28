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
@end

@implementation GridLayout
@synthesize startingTabWidth = _startingTabWidth;
@synthesize maxTabWidth = _maxTabWidth;

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
