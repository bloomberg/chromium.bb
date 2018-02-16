// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/ui/tab_grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_grid/grid_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kCellIdentifier = @"GridCellIdentifier";
}  // namespace

@interface GridViewController ()<UICollectionViewDataSource>
// A collection view of items in a grid format.
@property(nonatomic, weak) UICollectionView* collectionView;
// The local model backing the collection view.
@property(nonatomic, strong) NSMutableArray<GridItem*>* items;
// Selected index of the selected item. A negative value indicates that no item
// is selected.
@property(nonatomic, assign) NSInteger selectedIndex;
@end

@implementation GridViewController
@synthesize collectionView = _collectionView;
@synthesize items = _items;
@synthesize selectedIndex = _selectedIndex;

- (instancetype)init {
  if (self = [super init]) {
    _items = [[NSMutableArray<GridItem*> alloc] init];
  }
  return self;
}

- (void)loadView {
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.sectionInset = UIEdgeInsetsMake(20.0f, 20.0f, 20.0f, 20.0f);
  layout.itemSize = CGSizeMake(160.0f, 160.0f);
  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:layout];
  [collectionView registerClass:[GridCell class]
      forCellWithReuseIdentifier:kCellIdentifier];
  collectionView.dataSource = self;
  collectionView.backgroundColor = [UIColor blackColor];
  self.collectionView = collectionView;
  self.view = collectionView;
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return base::checked_cast<NSInteger>(self.items.count);
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  GridCell* cell = base::mac::ObjCCastStrict<GridCell>([collectionView
      dequeueReusableCellWithReuseIdentifier:kCellIdentifier
                                forIndexPath:indexPath]);
  [cell configureWithItem:self.items[indexPath.item] theme:GridCellThemeLight];
  return cell;
}

#pragma mark - GridConsumer

- (void)populateItems:(NSArray<GridItem*>*)items
        selectedIndex:(NSInteger)selectedIndex {
  self.items = [items mutableCopy];
  self.selectedIndex = selectedIndex;
  if ([self isViewLoaded]) {
    [self.collectionView
        reloadItemsAtIndexPaths:[self.collectionView
                                        indexPathsForVisibleItems]];
  }
}

#pragma mark - Private

- (void)setSelectedIndex:(NSInteger)selectedIndex {
  DCHECK_LT(selectedIndex, base::checked_cast<NSInteger>(self.items.count));
  _selectedIndex = selectedIndex;
}

@end
