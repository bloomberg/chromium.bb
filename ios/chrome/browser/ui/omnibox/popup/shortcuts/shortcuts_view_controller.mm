// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_view_controller.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_constants.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const NSInteger kNumberOfItemsPerRow = 4;
const CGFloat kLineSpacing = 30;
const CGFloat kItemSpacing = 10;
const CGFloat kTopInset = 10;
}  // namespace

#pragma mark - ShortcutCell

// A collection view subclass that contains a most visited tile.
@interface ShortcutCell : UICollectionViewCell

// The tile contained in the cell.
@property(nonatomic, strong) NTPMostVisitedTileView* tile;

@end

@implementation ShortcutCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _tile = [[NTPMostVisitedTileView alloc] init];
    _tile.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_tile];
    AddSameConstraints(self.contentView, _tile);
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.tile.titleLabel.text = nil;
}

@end

#pragma mark - ShortcutsViewController

@interface ShortcutsViewController ()<UICollectionViewDelegate,
                                      UICollectionViewDataSource>

@property(nonatomic, strong) UICollectionViewFlowLayout* layout;
@property(nonatomic, strong) UICollectionView* collectionView;
@property(nonatomic, strong)
    NSArray<ShortcutsMostVisitedItem*>* mostVisitedItems;

@end

@implementation ShortcutsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self.view addSubview:self.collectionView];
  self.collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.view, self.collectionView);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Calculate insets to center the items in the view.
  CGFloat widthInsets = (self.view.bounds.size.width -
                         kMostVisitedCellSize.width * kNumberOfItemsPerRow -
                         kItemSpacing * (kNumberOfItemsPerRow - 1)) /
                        2;
  self.layout.sectionInset =
      UIEdgeInsetsMake(kTopInset, widthInsets, 0, widthInsets);
}

#pragma mark - properties

- (UICollectionView*)collectionView {
  if (_collectionView) {
    return _collectionView;
  }

  _collectionView = [[UICollectionView alloc] initWithFrame:self.view.frame
                                       collectionViewLayout:self.layout];
  _collectionView.delegate = self;
  _collectionView.dataSource = self;
  _collectionView.backgroundColor = [UIColor clearColor];
  [_collectionView registerClass:[ShortcutCell class]
      forCellWithReuseIdentifier:NSStringFromClass([ShortcutCell class])];

  return _collectionView;
}

- (UICollectionViewFlowLayout*)layout {
  if (_layout) {
    return _layout;
  }

  _layout = [[UICollectionViewFlowLayout alloc] init];
  _layout.minimumLineSpacing = kLineSpacing;
  _layout.itemSize = kMostVisitedCellSize;
  return _layout;
}

#pragma mark - ShortcutsConsumer

- (void)mostVisitedShortcutsAvailable:
    (NSArray<ShortcutsMostVisitedItem*>*)items {
  self.mostVisitedItems = items;
  if (!self.viewLoaded) {
    return;
  }
  [self.collectionView reloadData];
}

- (void)faviconChangedForItem:(ShortcutsMostVisitedItem*)item {
  if (!self.viewLoaded) {
    return;
  }
  NSUInteger i = [self.mostVisitedItems indexOfObject:item];
  DCHECK(i != NSNotFound);
  [self.collectionView
      reloadItemsAtIndexPaths:@[ [NSIndexPath indexPathWithIndex:i] ]];
}

- (void)readingListBadgeUpdatedWithCount:(NSInteger)count {
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return kNumberOfItemsPerRow;
}

// The cell that is returned must be retrieved from a call to
// -dequeueReusableCellWithReuseIdentifier:forIndexPath:
- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  ShortcutCell* cell = [self.collectionView
      dequeueReusableCellWithReuseIdentifier:NSStringFromClass(
                                                 [ShortcutCell class])
                                forIndexPath:indexPath];
  ShortcutsMostVisitedItem* item = self.mostVisitedItems[indexPath.item];
  [cell.tile.faviconView configureWithAttributes:item.attributes];
  cell.tile.titleLabel.text = item.title;
  return cell;
}

@end
