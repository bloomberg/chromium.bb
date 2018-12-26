// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_local_commands.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_service_delegate.h"
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
  LegacySyncSwitchItem* syncSwitchItem =
      base::mac::ObjCCastStrict<LegacySyncSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:indexPath]);
  BOOL isOn = sender.isOn;
  GoogleServicesSettingsCommandID commandID =
      static_cast<GoogleServicesSettingsCommandID>(syncSwitchItem.commandID);
  switch (commandID) {
    case GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService:
      [self.serviceDelegate toggleAutocompleteSearchesServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDTogglePreloadPagesService:
      [self.serviceDelegate togglePreloadPagesServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleImproveChromeService:
      [self.serviceDelegate toggleImproveChromeServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService:
      [self.serviceDelegate toggleBetterSearchAndBrowsingServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDRestartAuthenticationFlow:
    case GoogleServicesSettingsReauthDialogAsSyncIsInAuthError:
    case GoogleServicesSettingsCommandIDShowPassphraseDialog:
    case GoogleServicesSettingsCommandIDNoOp:
      // Command ID not related with switch action.
      NOTREACHED();
      break;
  }
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
  if ([cell isKindOfClass:[LegacySyncSwitchCell class]]) {
    LegacySyncSwitchCell* switchCell =
        base::mac::ObjCCastStrict<LegacySyncSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
    switchCell.switchView.tag = [self tagForIndexPath:indexPath];
  }
  return cell;
}

#pragma mark - GoogleServicesSettingsConsumer

- (void)insertSections:(NSIndexSet*)sections {
  if (!self.collectionViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.collectionView insertSections:sections];
}

- (void)deleteSections:(NSIndexSet*)sections {
  if (!self.collectionViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.collectionView deleteSections:sections];
}

- (void)reloadSections:(NSIndexSet*)sections {
  if (!self.collectionViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.collectionView reloadSections:sections];
}

- (void)reloadItem:(CollectionViewItem*)item {
  if (!self.collectionViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  NSIndexPath* indexPath = [self.collectionViewModel indexPathForItem:item];
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
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
  if ([item isKindOfClass:[LegacySyncSwitchItem class]]) {
    return NO;
  } else if ([item isKindOfClass:[SettingsImageDetailTextItem class]]) {
    return YES;
  }
  // The highlight of an item should be explicitly defined. If the item can be
  // highlighted, then a command ID should be defined in
  // -[GoogleServicesSettingsViewController collectionView:
  // didSelectItemAtIndexPath:].
  NOTREACHED();
  return NO;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  GoogleServicesSettingsCommandID commandID =
      GoogleServicesSettingsCommandIDNoOp;
  if ([item isKindOfClass:[SettingsImageDetailTextItem class]]) {
    SettingsImageDetailTextItem* imageDetailTextItem =
        base::mac::ObjCCast<SettingsImageDetailTextItem>(item);
    commandID = static_cast<GoogleServicesSettingsCommandID>(
        imageDetailTextItem.commandID);
  } else {
    // A command ID should be defined when the cell is selected.
    NOTREACHED();
  }
  switch (commandID) {
    case GoogleServicesSettingsCommandIDRestartAuthenticationFlow:
      [self.localDispatcher restartAuthenticationFlow];
      break;
    case GoogleServicesSettingsReauthDialogAsSyncIsInAuthError:
      [self.localDispatcher openReauthDialogAsSyncIsInAuthError];
      break;
    case GoogleServicesSettingsCommandIDShowPassphraseDialog:
      [self.localDispatcher openPassphraseDialog];
      break;
    case GoogleServicesSettingsCommandIDNoOp:
    case GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService:
    case GoogleServicesSettingsCommandIDTogglePreloadPagesService:
    case GoogleServicesSettingsCommandIDToggleImproveChromeService:
    case GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService:
      // Command ID not related with cell selection.
      NOTREACHED();
      break;
  }
}

@end
