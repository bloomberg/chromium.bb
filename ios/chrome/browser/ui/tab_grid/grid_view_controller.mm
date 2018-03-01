// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/ui/tab_grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_grid/grid_item.h"
#import "ios/chrome/browser/ui/tab_grid/grid_layout.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kCellIdentifier = @"GridCellIdentifier";
// Creates an NSIndexPath with |index| in section 0.
NSIndexPath* CreateIndexPath(NSInteger index) {
  return [NSIndexPath indexPathForItem:index inSection:0];
}
}  // namespace

@interface GridViewController ()<GridCellDelegate,
                                 UICollectionViewDataSource,
                                 UICollectionViewDelegate>
// A collection view of items in a grid format.
@property(nonatomic, weak) UICollectionView* collectionView;
// The local model backing the collection view.
@property(nonatomic, strong) NSMutableArray<GridItem*>* items;
// Index of the selected item. This value is disregarded if |self.items| is
// empty. This bookkeeping is done to set the correct selection on
// |-viewWillAppear:|.
@property(nonatomic, assign) NSUInteger selectedIndex;
@end

@implementation GridViewController
// Public properties.
@synthesize theme = _theme;
@synthesize delegate = _delegate;
@synthesize imageDataSource = _imageDataSource;
// Private properties.
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
  GridLayout* layout = [[GridLayout alloc] init];
  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:layout];
  [collectionView registerClass:[GridCell class]
      forCellWithReuseIdentifier:kCellIdentifier];
  collectionView.dataSource = self;
  collectionView.delegate = self;
  collectionView.backgroundColor = [UIColor blackColor];
  if (@available(iOS 11, *))
    collectionView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentAlways;
  self.collectionView = collectionView;
  self.view = collectionView;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.collectionView reloadData];
  // Selection is invalid if there are no items.
  if (self.items.count == 0)
    return;
  [self.collectionView selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                                    animated:animated
                              scrollPosition:UICollectionViewScrollPositionTop];
}

#pragma mark - Public

- (UIScrollView*)gridView {
  return self.collectionView;
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
  cell.delegate = self;
  cell.theme = self.theme;
  GridItem* item = self.items[indexPath.item];
  cell.itemIdentifier = item.identifier;
  cell.title = item.title;
  NSString* itemIdentifier = item.identifier;
  [self.imageDataSource
      faviconForIdentifier:itemIdentifier
                completion:^(UIImage* icon) {
                  if (icon && cell.itemIdentifier == itemIdentifier)
                    cell.icon = icon;
                }];
  [self.imageDataSource
      snapshotForIdentifier:itemIdentifier
                 completion:^(UIImage* snapshot) {
                   if (snapshot && cell.itemIdentifier == itemIdentifier)
                     cell.snapshot = snapshot;
                 }];
  return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [self.delegate gridViewController:self didSelectItemAtIndex:indexPath.item];
}

#pragma mark - GridCellDelegate

- (void)closeButtonTappedForCell:(GridCell*)cell {
  NSInteger index = [self.collectionView indexPathForCell:cell].item;
  [self.delegate gridViewController:self
                didCloseItemAtIndex:base::checked_cast<NSUInteger>(index)];
}

#pragma mark - GridConsumer

- (void)populateItems:(NSArray<GridItem*>*)items
        selectedIndex:(NSUInteger)selectedIndex {
  self.items = [items mutableCopy];
  self.selectedIndex = selectedIndex;
  if (![self isViewVisible])
    return;
  [self.collectionView reloadData];
  [self.collectionView selectItemAtIndexPath:CreateIndexPath(selectedIndex)
                                    animated:YES
                              scrollPosition:UICollectionViewScrollPositionTop];
}

- (void)insertItem:(GridItem*)item
           atIndex:(NSUInteger)index
     selectedIndex:(NSUInteger)selectedIndex {
  [self.items insertObject:item atIndex:index];
  self.selectedIndex = selectedIndex;
  if (![self isViewVisible])
    return;
  [self.collectionView insertItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(selectedIndex)
                   animated:YES
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (void)removeItemAtIndex:(NSUInteger)index
            selectedIndex:(NSUInteger)selectedIndex {
  [self.items removeObjectAtIndex:index];
  self.selectedIndex = selectedIndex;
  if (![self isViewVisible])
    return;
  [self.collectionView deleteItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  if (self.items.count == 0)
    return;
  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(selectedIndex)
                   animated:YES
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (void)selectItemAtIndex:(NSUInteger)selectedIndex {
  self.selectedIndex = selectedIndex;
  if (![self isViewVisible])
    return;
  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(selectedIndex)
                   animated:YES
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (void)replaceItemAtIndex:(NSUInteger)index withItem:(GridItem*)item {
  self.items[index] = item;
  if (![self isViewVisible])
    return;
  [self.collectionView reloadItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
}

- (void)moveItemFromIndex:(NSUInteger)fromIndex
                  toIndex:(NSUInteger)toIndex
            selectedIndex:(NSUInteger)selectedIndex {
  GridItem* item = self.items[fromIndex];
  [self.items removeObjectAtIndex:fromIndex];
  [self.items insertObject:item atIndex:toIndex];
  self.selectedIndex = selectedIndex;
  if (![self isViewVisible])
    return;
  [self.collectionView moveItemAtIndexPath:CreateIndexPath(fromIndex)
                               toIndexPath:CreateIndexPath(toIndex)];
}

#pragma mark - Private

// If the view is not visible, there is no need to update the collection view.
- (BOOL)isViewVisible {
  // Invoking the view method causes the view to load (if it is not loaded)
  // which is unnecessary.
  return (self.isViewLoaded && self.view.window);
}

@end
