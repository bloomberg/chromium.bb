// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/physical_web_collection_view_controller.h"

#import <CoreLocation/CoreLocation.h>

#import "base/ios/weak_nsobject.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/metrics/user_metrics.h"
#include "components/google/core/browser/google_util.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/prefs/pref_member.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/physical_web/physical_web_constants.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPhysicalWeb = kSectionIdentifierEnumZero,
  SectionIdentifierLearnMore,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePhysicalWebSwitch = kItemTypeEnumZero,
  ItemTypeLearnMore,
};

}  // namespace

@interface PhysicalWebCollectionViewController () {
  // Pref for the Physical Web enabled state.
  IntegerPrefMember _physicalWebEnabled;
}

// Called when the preference switch is toggled on or off.
- (void)physicalWebSwitched:(UISwitch*)switchView;

// Updates the Physical Web preference.
- (void)updatePhysicalWebEnabled:(BOOL)enabled;
@end

@implementation PhysicalWebCollectionViewController

#pragma mark - Initialization

- (instancetype)initWithPrefs:(PrefService*)prefs {
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_PHYSICAL_WEB);
    _physicalWebEnabled.Init(prefs::kIosPhysicalWebEnabled, prefs);
    [self loadModel];
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style {
  NOTREACHED();
  return nil;
}

#pragma mark - Preference switch

+ (BOOL)shouldEnableForPreferenceState:(int)preferenceState {
  // Render the preference as "on" only if the preference is explicitly enabled.
  return (preferenceState == physical_web::kPhysicalWebOn);
}

- (void)physicalWebSwitched:(UISwitch*)switchView {
  [self updatePhysicalWebEnabled:switchView.isOn];
}

- (void)updatePhysicalWebEnabled:(BOOL)enabled {
  _physicalWebEnabled.SetValue(enabled ? physical_web::kPhysicalWebOn
                                       : physical_web::kPhysicalWebOff);

  physical_web::PhysicalWebDataSource* dataSource =
      GetApplicationContext()->GetPhysicalWebDataSource();

  if (enabled) {
    base::RecordAction(
        base::UserMetricsAction("PhysicalWeb.Prefs.FeatureEnabled"));
    if (dataSource) {
      dataSource->StartDiscovery(true);
    }
  } else {
    base::RecordAction(
        base::UserMetricsAction("PhysicalWeb.Prefs.FeatureDisabled"));
    if (dataSource) {
      dataSource->StopDiscovery();
    }
  }
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypePhysicalWebSwitch: {
      CollectionViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(physicalWebSwitched:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
  }

  return cell;
}

#pragma mark - UICollectionViewDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHighlightItemAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypePhysicalWebSwitch:
    case ItemTypeLearnMore:
      return NO;
    default:
      return [super collectionView:collectionView
          shouldHighlightItemAtIndexPath:indexPath];
  }
}

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierPhysicalWeb];

  NSString* switchLabelText =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_PHYSICAL_WEB);

  CollectionViewSwitchItem* switchItem = [[[CollectionViewSwitchItem alloc]
      initWithType:ItemTypePhysicalWebSwitch] autorelease];
  switchItem.text = switchLabelText;
  switchItem.on = [PhysicalWebCollectionViewController
      shouldEnableForPreferenceState:_physicalWebEnabled.GetValue()];
  [model addItem:switchItem
      toSectionWithIdentifier:SectionIdentifierPhysicalWeb];

  [model addSectionWithIdentifier:SectionIdentifierLearnMore];

  NSString* learnMoreText =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_PHYSICAL_WEB_DETAILS);

  CollectionViewFooterItem* learnMore = [[[CollectionViewFooterItem alloc]
      initWithType:ItemTypeLearnMore] autorelease];
  learnMore.text = learnMoreText;
  learnMore.linkURL = GURL(kPhysicalWebLearnMoreURL);
  learnMore.linkDelegate = self;
  learnMore.accessibilityTraits = UIAccessibilityTraitButton;
  [model addItem:learnMore toSectionWithIdentifier:SectionIdentifierLearnMore];
}

#pragma mark - MDCCollectionViewStylingDelegate

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierLearnMore:
      // Display the Learn More footer in the default style with no "card" UI
      // and no section padding.
      return MDCCollectionViewCellStyleDefault;
    default:
      return self.styler.cellStyle;
  }
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeLearnMore:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierLearnMore:
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
    case ItemTypePhysicalWebSwitch:
      return YES;
    default:
      return NO;
  }
}

@end
