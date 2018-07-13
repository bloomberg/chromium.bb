// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_collapsible_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller_model_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation GoogleServicesSettingsViewController

@synthesize modelDelegate = _modelDelegate;
@synthesize presentationDelegate = _presentationDelegate;

- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style {
  self = [super initWithLayout:layout style:style];
  if (self) {
    self.collectionViewAccessibilityIdentifier =
        @"google_services_settings_view_controller";
    self.title = l10n_util::GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE);
  }
  return self;
}

// Collapse/expand a section at |sectionIndex|.
- (void)toggleSectionWithIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0, indexPath.row);
  NSMutableArray* cellIndexPathsToDeleteOrInsert = [NSMutableArray array];
  CollectionViewModel* model = self.collectionViewModel;
  NSInteger sectionIdentifier =
      [model sectionIdentifierForSection:indexPath.section];
  NSEnumerator* itemEnumerator =
      [[model itemsInSectionWithIdentifier:sectionIdentifier] objectEnumerator];
  // The first item is the title item that does the collapse/expand. This item
  // should always be visible and should be skipped.
  [itemEnumerator nextObject];
  for (ListItem* item in itemEnumerator) {
    NSIndexPath* tabIndexPath = [model indexPathForItem:item];
    [cellIndexPathsToDeleteOrInsert addObject:tabIndexPath];
  }

  BOOL shouldCollapse = ![model sectionIsCollapsed:sectionIdentifier];
  void (^tableUpdates)(void) = ^{
    if (!shouldCollapse) {
      [model setSection:sectionIdentifier collapsed:NO];
      [self.collectionView
          insertItemsAtIndexPaths:cellIndexPathsToDeleteOrInsert];
    } else {
      [model setSection:sectionIdentifier collapsed:YES];
      [self.collectionView
          deleteItemsAtIndexPaths:cellIndexPathsToDeleteOrInsert];
    }
  };
  [self.collectionView performBatchUpdates:tableUpdates completion:nil];

  SettingsCollapsibleItem* item = [model itemAtIndexPath:indexPath];
  item.collapsed = shouldCollapse;
  SettingsCollapsibleCell* cell = (SettingsCollapsibleCell*)[self.collectionView
      cellForItemAtIndexPath:indexPath];
  [cell setCollapsed:shouldCollapse animated:YES];
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  [self.modelDelegate googleServicesSettingsViewControllerLoadModel:self];
}

#pragma mark - UIViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self reloadData];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        googleServicesSettingsViewControllerDidRemove:self];
  }
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];
  CGFloat width =
      CGRectGetWidth(collectionView.bounds) - inset.left - inset.right;
  return [item.cellClass cr_preferredHeightForWidth:width forItem:item];
}

#pragma mark - UICollectionViewDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHighlightItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView
      shouldHighlightItemAtIndexPath:indexPath];
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  return ![item isKindOfClass:[SyncSwitchItem class]];
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[SettingsCollapsibleItem class]]) {
    [self toggleSectionWithIndexPath:indexPath];
  }
}

@end
