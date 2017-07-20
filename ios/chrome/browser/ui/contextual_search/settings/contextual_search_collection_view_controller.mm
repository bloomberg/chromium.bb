// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/contextual_search/settings/contextual_search_collection_view_controller.h"

#import "base/mac/foundation_util.h"
#include "components/google/core/browser/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/contextual_search/touch_to_search_permissions_mediator.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTouchToSearch = kSectionIdentifierEnumZero,
  SectionIdentifierFooter,
};

typedef NS_ENUM(NSUInteger, ItemType) {
  ItemTypeTouchToSearchSwitch = kItemTypeEnumZero,
  ItemTypeFooter,
};

}  // namespace

@interface ContextualSearchCollectionViewController () {
  // Permissions interface for Touch-to-Search.
  TouchToSearchPermissionsMediator* _touchToSearchPermissions;
}

// Returns the switch item to use for the touch to search setting.
- (CollectionViewSwitchItem*)touchToSearchSwitchItem;

// Returns the item to use for the footer.
- (CollectionViewFooterItem*)footerItem;

// Called when the touch to search switch is toggled.
- (void)switchToggled:(id)sender;

@end

@implementation ContextualSearchCollectionViewController

#pragma mark - Initialization

- (instancetype)initWithPermissions:
    (TouchToSearchPermissionsMediator*)touchToSearchPermissions {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_CONTEXTUAL_SEARCH_TITLE);
    _touchToSearchPermissions = touchToSearchPermissions;
    self.collectionViewAccessibilityIdentifier = @"Contextual Search";
    [self loadModel];
  }
  return self;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  return [self initWithPermissions:[[TouchToSearchPermissionsMediator alloc]
                                       initWithBrowserState:browserState]];
}

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierTouchToSearch];
  [model addItem:[self touchToSearchSwitchItem]
      toSectionWithIdentifier:SectionIdentifierTouchToSearch];

  [model addSectionWithIdentifier:SectionIdentifierFooter];
  [model addItem:[self footerItem]
      toSectionWithIdentifier:SectionIdentifierFooter];
}

- (CollectionViewSwitchItem*)touchToSearchSwitchItem {
  CollectionViewSwitchItem* item = [[CollectionViewSwitchItem alloc]
      initWithType:ItemTypeTouchToSearchSwitch];
  item.text = l10n_util::GetNSString(IDS_IOS_CONTEXTUAL_SEARCH_TITLE);
  item.on =
      ([_touchToSearchPermissions preferenceState] != TouchToSearch::DISABLED);
  return item;
}

- (CollectionViewFooterItem*)footerItem {
  NSString* footerText =
      [NSString stringWithFormat:@"%@ BEGIN_LINK%@END_LINK",
                                 l10n_util::GetNSString(
                                     IDS_IOS_CONTEXTUAL_SEARCH_DESCRIPTION),
                                 l10n_util::GetNSString(IDS_LEARN_MORE)];
  GURL learnMoreURL = google_util::AppendGoogleLocaleParam(
      GURL(l10n_util::GetStringUTF8(IDS_IOS_CONTEXTUAL_SEARCH_LEARN_MORE_URL)),
      GetApplicationContext()->GetApplicationLocale());

  CollectionViewFooterItem* item =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeFooter];
  item.text = footerText;
  item.linkURL = learnMoreURL;
  item.linkDelegate = self;
  return item;
}

- (void)switchToggled:(id)sender {
  NSIndexPath* switchPath = [self.collectionViewModel
      indexPathForItemType:ItemTypeTouchToSearchSwitch
         sectionIdentifier:SectionIdentifierTouchToSearch];

  CollectionViewSwitchItem* switchItem =
      base::mac::ObjCCastStrict<CollectionViewSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:switchPath]);
  CollectionViewSwitchCell* switchCell =
      base::mac::ObjCCastStrict<CollectionViewSwitchCell>(
          [self.collectionView cellForItemAtIndexPath:switchPath]);

  // Update the model and the preference with the current value of the switch.
  DCHECK_EQ(switchCell.switchView, sender);
  BOOL isOn = switchCell.switchView.isOn;
  switchItem.on = isOn;
  TouchToSearch::TouchToSearchPreferenceState state =
      isOn ? TouchToSearch::ENABLED : TouchToSearch::DISABLED;
  [_touchToSearchPermissions setPreferenceState:state];
}

#pragma mark - UICollectionViewDelegate

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeTouchToSearchSwitch) {
    // Have the switch send a message on UIControlEventValueChanged.
    CollectionViewSwitchCell* switchCell =
        base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchToggled:)
                    forControlEvents:UIControlEventValueChanged];
  }

  return cell;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeFooter:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierFooter:
      // Display the Learn More footer in the default style with no "card" UI
      // and no section padding.
      return MDCCollectionViewCellStyleDefault;
    default:
      return self.styler.cellStyle;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierFooter:
      // Display the Learn More footer without any background image or
      // shadowing.
      return YES;
    default:
      return NO;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeFooter:
    case ItemTypeTouchToSearchSwitch:
      return YES;
    default:
      return NO;
  }
}

@end
