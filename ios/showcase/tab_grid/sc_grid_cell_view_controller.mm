// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_grid_cell_view_controller.h"

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

@interface SCGridCellViewController ()<UICollectionViewDataSource,
                                       UICollectionViewDelegateFlowLayout>
@property(nonatomic, strong) NSArray* items;
@property(nonatomic, strong) NSArray* sizes;
@end

@implementation SCGridCellViewController
@synthesize items = _items;
@synthesize sizes = _sizes;

- (instancetype)init {
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.sectionInset = UIEdgeInsetsMake(20.0f, 20.0f, 20.0f, 20.0f);
  if (self = [super initWithCollectionViewLayout:layout]) {
    self.collectionView.dataSource = self;
    self.collectionView.delegate = self;
    [self.collectionView registerClass:[GridCell class]
            forCellWithReuseIdentifier:kCellIdentifier];
    self.collectionView.backgroundColor = [UIColor blackColor];
    GridItem* item1 = [[GridItem alloc] init];
    item1.title = @"Basecamp: project with an attitude.";
    GridItem* item2 = [[GridItem alloc] init];
    item2.title = @"Basecamp: project with an attitude.";
    GridItem* item3 = [[GridItem alloc] init];
    item3.title = @"Basecamp: project with an attitude.";
    GridItem* item4 = [[GridItem alloc] init];
    item4.title = @"Basecamp: project with an attitude.";
    self.items = @[ item1, item2, item3, item4 ];
    self.sizes = @[
      [NSValue valueWithCGSize:CGSizeMake(140.0f, 168.0f)],
      [NSValue valueWithCGSize:CGSizeMake(180.0f, 208.0f)],
      [NSValue valueWithCGSize:CGSizeMake(220.0f, 248.0f)],
      [NSValue valueWithCGSize:CGSizeMake(180.0f, 208.0f)],
    ];
  }
  return self;
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
  GridCellTheme theme = GridCellThemeLight;
  if (indexPath.item == 1)
    theme = GridCellThemeDark;
  [cell configureWithItem:self.items[indexPath.item] theme:theme];
  return cell;
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  return [self.sizes[indexPath.item] CGSizeValue];
}

@end
