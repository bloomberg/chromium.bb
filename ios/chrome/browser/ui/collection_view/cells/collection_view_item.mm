// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

#import "base/logging.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CollectionViewItem

@synthesize type = _type;
@synthesize cellClass = _cellClass;
@synthesize accessibilityIdentifier = _accessibilityIdentifier;

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super init])) {
    _type = type;
    _cellClass = [MDCCollectionViewCell class];
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)setCellClass:(Class)cellClass {
  DCHECK([cellClass isSubclassOfClass:[MDCCollectionViewCell class]]);
  _cellClass = cellClass;
}

- (void)configureCell:(MDCCollectionViewCell*)cell {
  DCHECK([cell class] == self.cellClass);
  cell.accessibilityTraits = self.accessibilityTraits;
  cell.accessibilityIdentifier = self.accessibilityIdentifier;
}

@end
