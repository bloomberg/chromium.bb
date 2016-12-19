// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@implementation CollectionViewController {
  // The implementation of this controller follows the guidelines from
  // https://github.com/material-components/material-components-ios/tree/develop/components/AppBar
  base::scoped_nsobject<MDCAppBar> _appBar;
  base::scoped_nsobject<CollectionViewModel> _collectionViewModel;
}

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style {
  UICollectionViewLayout* layout =
      [[[MDCCollectionViewFlowLayout alloc] init] autorelease];
  self = [super initWithCollectionViewLayout:layout];
  if (self) {
    if (style == CollectionViewControllerStyleAppBar) {
      _appBar.reset([[MDCAppBar alloc] init]);
      [self addChildViewController:_appBar.get().headerViewController];
    }
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Configure the app bar, if there is one.
  if (self.appBar) {
    // Configure the app bar style.
    ConfigureAppBarWithCardStyle(self.appBar);
    // Set the header view's tracking scroll view.
    self.appBar.headerViewController.headerView.trackingScrollView =
        self.collectionView;
    // After all other views have been registered.
    [self.appBar addSubviewsToParent];
  }
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.appBar.headerViewController;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.appBar.headerViewController;
}

- (MDCAppBar*)appBar {
  return _appBar.get();
}

- (CollectionViewModel*)collectionViewModel {
  return _collectionViewModel.get();
}

- (void)loadModel {
  _collectionViewModel.reset([[CollectionViewModel alloc] init]);
}

- (void)reconfigureCellsForItems:(NSArray*)items
         inSectionWithIdentifier:(NSInteger)sectionIdentifier {
  for (CollectionViewItem* item in items) {
    NSIndexPath* indexPath =
        [_collectionViewModel indexPathForItem:item
                       inSectionWithIdentifier:sectionIdentifier];
    MDCCollectionViewCell* cell =
        base::mac::ObjCCastStrict<MDCCollectionViewCell>(
            [self.collectionView cellForItemAtIndexPath:indexPath]);

    // |cell| may be nil if the row is not currently on screen.
    if (cell) {
      [item configureCell:cell];
    }
  }
}

#pragma mark MDCCollectionViewEditingDelegate

- (void)collectionView:(UICollectionView*)collectionView
    willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  // Check that the parent class doesn't implement this method. Otherwise, it
  // would need to be called here.
  DCHECK([self isKindOfClass:[MDCCollectionViewController class]]);
  DCHECK(![MDCCollectionViewController instancesRespondToSelector:_cmd]);

  // Sort and enumerate in reverse order to delete the items from the collection
  // view model.
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  for (NSIndexPath* indexPath in [sortedIndexPaths reverseObjectEnumerator]) {
    NSInteger sectionIdentifier =
        [_collectionViewModel sectionIdentifierForSection:indexPath.section];
    NSInteger itemType = [_collectionViewModel itemTypeForIndexPath:indexPath];
    NSUInteger index =
        [_collectionViewModel indexInItemTypeForIndexPath:indexPath];
    [_collectionViewModel removeItemWithType:itemType
                   fromSectionWithIdentifier:sectionIdentifier
                                     atIndex:index];
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    willMoveItemAtIndexPath:(NSIndexPath*)indexPath
                toIndexPath:(NSIndexPath*)newIndexPath {
  // Check that the parent class doesn't implement this method. Otherwise, it
  // would need to be called here.
  DCHECK([self isKindOfClass:[MDCCollectionViewController class]]);
  DCHECK(![MDCCollectionViewController instancesRespondToSelector:_cmd]);

  // Retain the item to be able to move it.
  base::scoped_nsobject<CollectionViewItem> item(
      [[self.collectionViewModel itemAtIndexPath:indexPath] retain]);

  // Item coordinates.
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  NSUInteger indexInItemType =
      [self.collectionViewModel indexInItemTypeForIndexPath:indexPath];

  // Move the item.
  [self.collectionViewModel removeItemWithType:itemType
                     fromSectionWithIdentifier:sectionIdentifier
                                       atIndex:indexInItemType];
  NSInteger section = [self.collectionViewModel
      sectionIdentifierForSection:newIndexPath.section];
  [self.collectionViewModel insertItem:item
               inSectionWithIdentifier:section
                               atIndex:newIndexPath.item];
}

#pragma mark UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return [_collectionViewModel numberOfSections];
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return [_collectionViewModel numberOfItemsInSection:section];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item = [_collectionViewModel itemAtIndexPath:indexPath];
  Class cellClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(cellClass);
  [self.collectionView registerClass:cellClass
          forCellWithReuseIdentifier:reuseIdentifier];
  MDCCollectionViewCell* cell = [self.collectionView
      dequeueReusableCellWithReuseIdentifier:reuseIdentifier
                                forIndexPath:indexPath];
  [item configureCell:cell];
  return cell;
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item = nil;
  UIAccessibilityTraits traits = UIAccessibilityTraitNone;
  if ([kind isEqualToString:UICollectionElementKindSectionHeader]) {
    item = [_collectionViewModel headerForSection:indexPath.section];
    traits = UIAccessibilityTraitHeader;
  } else if ([kind isEqualToString:UICollectionElementKindSectionFooter]) {
    item = [_collectionViewModel footerForSection:indexPath.section];
  } else {
    return [super collectionView:collectionView
        viewForSupplementaryElementOfKind:kind
                              atIndexPath:indexPath];
  }

  Class cellClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(cellClass);
  [self.collectionView registerClass:cellClass
          forSupplementaryViewOfKind:kind
                 withReuseIdentifier:reuseIdentifier];
  MDCCollectionViewCell* cell = [self.collectionView
      dequeueReusableSupplementaryViewOfKind:kind
                         withReuseIdentifier:reuseIdentifier
                                forIndexPath:indexPath];
  [item configureCell:cell];
  cell.accessibilityTraits |= traits;
  return cell;
};

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  CollectionViewItem* item = [_collectionViewModel headerForSection:section];

  if (item) {
    // TODO(crbug.com/635604): Support arbitrary sized headers.
    return CGSizeMake(0, MDCCellDefaultOneLineHeight);
  }
  return CGSizeZero;
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForFooterInSection:(NSInteger)section {
  CollectionViewItem* item = [_collectionViewModel footerForSection:section];

  if (item) {
    // TODO(crbug.com/635604): Support arbitrary sized footers.
    return CGSizeMake(0, MDCCellDefaultOneLineHeight);
  }
  return CGSizeZero;
}

#pragma mark UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView =
      self.appBar.headerViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidScroll];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView =
      self.appBar.headerViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDecelerating];
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  MDCFlexibleHeaderView* headerView =
      self.appBar.headerViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDraggingWillDecelerate:decelerate];
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  MDCFlexibleHeaderView* headerView =
      self.appBar.headerViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView
        trackingScrollViewWillEndDraggingWithVelocity:velocity
                                  targetContentOffset:targetContentOffset];
  }
}

@end
