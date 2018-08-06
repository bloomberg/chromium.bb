// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_collapsible_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller_model_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constants used to convert NSIndexPath into a tag. Used as:
// item + section * kSectionOffset
constexpr NSInteger kSectionOffset = 1000;

}  // namespace

@implementation GoogleServicesSettingsViewController

@synthesize presentationDelegate = _presentationDelegate;
@synthesize modelDelegate = _modelDelegate;
@synthesize commandHandler = _commandHandler;

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
#pragma mark - Private

- (NSInteger)tagForIndexPath:(NSIndexPath*)indexPath {
  return indexPath.item + indexPath.section * kSectionOffset;
}

- (NSIndexPath*)indexPathForTag:(NSInteger)tag {
  NSInteger section = tag / kSectionOffset;
  NSInteger item = tag - (section * kSectionOffset);
  return [NSIndexPath indexPathForItem:item inSection:section];
}

- (void)switchAction:(UISwitch*)sender {
  NSIndexPath* indexPath = [self indexPathForTag:sender.tag];
  SyncSwitchItem* syncSwitchItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
      [self.collectionViewModel itemAtIndexPath:indexPath]);
  BOOL isOn = sender.isOn;
  GoogleServicesSettingsCommandID commandID =
      static_cast<GoogleServicesSettingsCommandID>(syncSwitchItem.commandID);
  switch (commandID) {
    case GoogleServicesSettingsCommandIDNoOp:
    case GoogleServicesSettingsCommandIDOpenGoogleActivityPage:
    case GoogleServicesSettingsCommandIDOpenEncryptionDialog:
    case GoogleServicesSettingsCommandIDOpenManageSyncedDataPage:
      NOTREACHED();
      break;
    case GoogleServicesSettingsCommandIDToggleSyncEverything:
      [self.commandHandler toggleSyncEverythingWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleBookmarkSync:
      [self.commandHandler toggleBookmarksSyncWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleHistorySync:
      [self.commandHandler toggleHistorySyncWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDTogglePasswordsSync:
      [self.commandHandler togglePasswordsSyncWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleOpenTabsSync:
      [self.commandHandler toggleOpenTabSyncWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleAutofillSync:
      [self.commandHandler toggleAutofillWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleSettingsSync:
      [self.commandHandler toggleSettingsWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleReadingListSync:
      [self.commandHandler toggleReadingListWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleActivityAndInteractionsService:
      [self.commandHandler toggleActivityAndInteractionsServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService:
      [self.commandHandler toggleAutocompleteSearchesServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDTogglePreloadPagesService:
      [self.commandHandler togglePreloadPagesServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleImproveChromeService:
      [self.commandHandler toggleImproveChromeServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService:
      [self.commandHandler toggleBetterSearchAndBrowsingServiceWithValue:isOn];
      break;
  }
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
  if ([cell isKindOfClass:[SyncSwitchCell class]]) {
    SyncSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SyncSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
    switchCell.switchView.tag = [self tagForIndexPath:indexPath];
  }
  return cell;
}

- (void)reloadSections:(NSIndexSet*)sections {
  [self.collectionView reloadSections:sections];
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
  if ([item isKindOfClass:[SyncSwitchItem class]]) {
    return NO;
  } else if ([item isKindOfClass:[SettingsCollapsibleItem class]]) {
    return YES;
  } else if ([item isKindOfClass:[CollectionViewTextItem class]]) {
    CollectionViewTextItem* textItem =
        base::mac::ObjCCast<CollectionViewTextItem>(item);
    return textItem.commandID != 0;
  }
  return NO;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[SettingsCollapsibleItem class]]) {
    [self toggleSectionWithIndexPath:indexPath];
    return;
  }
  CollectionViewTextItem* textItem =
      base::mac::ObjCCastStrict<CollectionViewTextItem>(
          [self.collectionViewModel itemAtIndexPath:indexPath]);
  GoogleServicesSettingsCommandID commandID =
      static_cast<GoogleServicesSettingsCommandID>(textItem.commandID);
  switch (commandID) {
    case GoogleServicesSettingsCommandIDOpenGoogleActivityPage:
      [self.commandHandler openGoogleActivityPage];
      break;
    case GoogleServicesSettingsCommandIDOpenEncryptionDialog:
      [self.commandHandler openEncryptionDialog];
      break;
    case GoogleServicesSettingsCommandIDOpenManageSyncedDataPage:
      [self.commandHandler openManageSyncedDataPage];
      break;
    case GoogleServicesSettingsCommandIDNoOp:
    case GoogleServicesSettingsCommandIDToggleSyncEverything:
    case GoogleServicesSettingsCommandIDToggleBookmarkSync:
    case GoogleServicesSettingsCommandIDToggleHistorySync:
    case GoogleServicesSettingsCommandIDTogglePasswordsSync:
    case GoogleServicesSettingsCommandIDToggleOpenTabsSync:
    case GoogleServicesSettingsCommandIDToggleAutofillSync:
    case GoogleServicesSettingsCommandIDToggleSettingsSync:
    case GoogleServicesSettingsCommandIDToggleReadingListSync:
    case GoogleServicesSettingsCommandIDToggleActivityAndInteractionsService:
    case GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService:
    case GoogleServicesSettingsCommandIDTogglePreloadPagesService:
    case GoogleServicesSettingsCommandIDToggleImproveChromeService:
    case GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService:
      NOTREACHED();
      break;
  }
}

@end
