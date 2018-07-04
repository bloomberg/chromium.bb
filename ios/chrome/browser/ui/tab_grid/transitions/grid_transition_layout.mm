// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_layout.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/logging.h"

@interface GridTransitionLayout ()
@property(nonatomic, readwrite) NSArray<GridTransitionLayoutItem*>* items;
@property(nonatomic, readwrite) GridTransitionLayoutItem* activeItem;
@property(nonatomic, readwrite) GridTransitionLayoutItem* selectionItem;
@end

@implementation GridTransitionLayout
@synthesize activeItem = _activeItem;
@synthesize selectionItem = _selectionItem;
@synthesize items = _items;
@synthesize expandedRect = _expandedRect;

+ (instancetype)layoutWithItems:(NSArray<GridTransitionLayoutItem*>*)items
                     activeItem:(GridTransitionLayoutItem*)activeItem
                  selectionItem:(GridTransitionLayoutItem*)selectionItem {
  DCHECK(items);
  GridTransitionLayout* layout = [[GridTransitionLayout alloc] init];
  layout.items = items;
  layout.activeItem = activeItem;
  layout.selectionItem = selectionItem;
  return layout;
}

- (void)setActiveItem:(GridTransitionLayoutItem*)activeItem {
  DCHECK(!activeItem || [self.items containsObject:activeItem]);
  _activeItem = activeItem;
}

@end

@interface GridTransitionLayoutItem ()
@property(nonatomic, readwrite) UICollectionViewCell* cell;
@property(nonatomic, readwrite) UIView* auxillaryView;
@property(nonatomic, readwrite) UICollectionViewLayoutAttributes* attributes;
@end

@implementation GridTransitionLayoutItem
@synthesize cell = _cell;
@synthesize auxillaryView = _auxillaryView;
@synthesize attributes = _attributes;

+ (instancetype)itemWithCell:(UICollectionViewCell*)cell
               auxillaryView:(UIView*)auxillaryView
                  attributes:(UICollectionViewLayoutAttributes*)attributes {
  DCHECK(cell);
  DCHECK(!cell.superview);
  DCHECK(!auxillaryView || [auxillaryView isDescendantOfView:cell]);
  GridTransitionLayoutItem* item = [[GridTransitionLayoutItem alloc] init];
  item.cell = cell;
  item.auxillaryView = auxillaryView;
  item.attributes = attributes;
  return item;
}
@end
