// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid/grid_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_layout.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

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
  collectionView.backgroundColor = UIColorFromRGB(kGridBackgroundColor);
  if (@available(iOS 11, *))
    collectionView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentAlways;
  self.collectionView = collectionView;
  self.view = collectionView;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.collectionView reloadData];
  self.emptyStateView.hidden = (self.items.count > 0);
  // Selection is invalid if there are no items.
  if (self.items.count == 0)
    return;
  [self.collectionView selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                                    animated:animated
                              scrollPosition:UICollectionViewScrollPositionTop];
  // Update the delegate, in case it wasn't set when |items| was populated.
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
}

#pragma mark - Public

- (UIScrollView*)gridView {
  return self.collectionView;
}

- (void)setEmptyStateView:(UIView*)emptyStateView {
  self.collectionView.backgroundView = emptyStateView;
  // The emptyStateView is hidden when there are items, and shown when the
  // collectionView is empty.
  self.collectionView.backgroundView.hidden = (self.items.count > 0);
}

- (UIView*)emptyStateView {
  return self.collectionView.backgroundView;
}

- (BOOL)isGridEmpty {
  return self.items.count == 0;
}

- (BOOL)isSelectedCellVisible {
  if (self.collectionView.indexPathsForSelectedItems.count == 0)
    return NO;
  return [self.collectionView.indexPathsForVisibleItems
      containsObject:self.collectionView.indexPathsForSelectedItems
                         .firstObject];
}

- (GridTransitionLayout*)transitionLayout {
  [self.collectionView layoutIfNeeded];
  NSMutableArray<GridTransitionLayoutItem*>* items =
      [[NSMutableArray alloc] init];
  GridTransitionLayoutItem* selectedItem;
  for (NSIndexPath* path in self.collectionView.indexPathsForVisibleItems) {
    GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
        [self.collectionView cellForItemAtIndexPath:path]);
    UICollectionViewLayoutAttributes* attributes =
        [self.collectionView layoutAttributesForItemAtIndexPath:path];
    // Normalize frame to window coordinates. The attributes class applies this
    // change to the other properties such as center, bounds, etc.
    attributes.frame =
        [self.collectionView convertRect:attributes.frame toView:nil];
    GridTransitionLayoutItem* item =
        [GridTransitionLayoutItem itemWithCell:[cell proxyForTransitions]
                                    attributes:attributes];
    [items addObject:item];
    if (cell.selected) {
      selectedItem = item;
    }
  }
  return [GridTransitionLayout layoutWithItems:items selectedItem:selectedItem];
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
  cell.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@%ld", kGridCellIdentifierPrefix,
                                 base::checked_cast<long>(indexPath.item)];
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
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
}

- (void)insertItem:(GridItem*)item
           atIndex:(NSUInteger)index
     selectedIndex:(NSUInteger)selectedIndex {
  auto performDataSourceUpdates = ^{
    [self.items insertObject:item atIndex:index];
    self.selectedIndex = selectedIndex;
  };
  if (![self isViewVisible]) {
    performDataSourceUpdates();
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
    return;
  }
  auto performAllUpdates = ^{
    performDataSourceUpdates();
    self.emptyStateView.hidden = YES;
    [self.collectionView insertItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  };
  auto completion = ^(BOOL finished) {
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(selectedIndex)
                     animated:YES
               scrollPosition:UICollectionViewScrollPositionNone];
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  };
  [self.collectionView performBatchUpdates:performAllUpdates
                                completion:completion];
}

- (void)removeItemAtIndex:(NSUInteger)index
            selectedIndex:(NSUInteger)selectedIndex {
  auto performDataSourceUpdates = ^{
    [self.items removeObjectAtIndex:index];
    self.selectedIndex = selectedIndex;
  };
  if (![self isViewVisible]) {
    performDataSourceUpdates();
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
    return;
  }
  auto performAllUpdates = ^{
    performDataSourceUpdates();
    [self.collectionView deleteItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
    if (self.items.count == 0) {
      [self animateEmptyStateIntoView];
    }
  };
  auto completion = ^(BOOL finished) {
    if (self.items.count > 0) {
      [self.collectionView
          selectItemAtIndexPath:CreateIndexPath(selectedIndex)
                       animated:YES
                 scrollPosition:UICollectionViewScrollPositionNone];
    }
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  };
  [self.collectionView performBatchUpdates:performAllUpdates
                                completion:completion];
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
  auto performDataSourceUpdates = ^{
    GridItem* item = self.items[fromIndex];
    [self.items removeObjectAtIndex:fromIndex];
    [self.items insertObject:item atIndex:toIndex];
    self.selectedIndex = selectedIndex;
  };
  if (![self isViewVisible]) {
    performDataSourceUpdates();
    return;
  }
  auto performAllUpdates = ^{
    performDataSourceUpdates();
    [self.collectionView moveItemAtIndexPath:CreateIndexPath(fromIndex)
                                 toIndexPath:CreateIndexPath(toIndex)];
  };
  auto completion = ^(BOOL finished) {
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(selectedIndex)
                     animated:YES
               scrollPosition:UICollectionViewScrollPositionNone];
  };
  [self.collectionView performBatchUpdates:performAllUpdates
                                completion:completion];
}

#pragma mark - Private

// If the view is not visible, there is no need to update the collection view.
- (BOOL)isViewVisible {
  // Invoking the view method causes the view to load (if it is not loaded)
  // which is unnecessary.
  return (self.isViewLoaded && self.view.window);
}

// Animates the empty state into view.
- (void)animateEmptyStateIntoView {
  // TODO(crbug.com/820410) : Polish the animation, and put constants where they
  // belong.
  CGFloat scale = 0.8f;
  CGAffineTransform originalTransform = self.emptyStateView.transform;
  self.emptyStateView.transform =
      CGAffineTransformScale(self.emptyStateView.transform, scale, scale);
  self.emptyStateView.alpha = 0.0f;
  self.emptyStateView.hidden = NO;
  [UIView animateWithDuration:1.0f
                        delay:0.0f
       usingSpringWithDamping:1.0f
        initialSpringVelocity:0.7f
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
                     self.emptyStateView.alpha = 1.0f;
                     self.emptyStateView.transform = originalTransform;
                   }
                   completion:nil];
}

@end
