// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings_collection_view_controller.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/prefs/pref_observer_bridge.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/block_popups_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/translate_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSettings = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSettingsBlockPopups = kItemTypeEnumZero,
  ItemTypeSettingsTranslate,
};

}  // namespace

@interface ContentSettingsCollectionViewController ()<PrefObserverDelegate,
                                                      BooleanObserver> {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // The observable boolean that binds to the "Disable Popups" setting state.
  base::scoped_nsobject<ContentSettingBackedBoolean> _disablePopupsSetting;

  // Updatable Items
  base::scoped_nsobject<CollectionViewDetailItem> _blockPopupsDetailItem;
  base::scoped_nsobject<CollectionViewDetailItem> _translateDetailItem;
}

// Returns the value for the default setting with ID |settingID|.
- (ContentSetting)getContentSetting:(ContentSettingsType)settingID;

// Helpers to create collection view items.
- (id)blockPopupsItem;
- (id)translateItem;

@end

@implementation ContentSettingsCollectionViewController {
  ios::ChromeBrowserState* browserState_;  // weak
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    browserState_ = browserState;
    self.title = l10n_util::GetNSString(IDS_IOS_CONTENT_SETTINGS_TITLE);

    _prefChangeRegistrar.Init(browserState->GetPrefs());
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(prefs::kEnableTranslate,
                                                     &_prefChangeRegistrar);

    HostContentSettingsMap* settingsMap =
        ios::HostContentSettingsMapFactory::GetForBrowserState(browserState);
    _disablePopupsSetting.reset([[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:CONTENT_SETTINGS_TYPE_POPUPS
                              inverted:YES]);
    [_disablePopupsSetting setObserver:self];

    [self loadModel];
  }
  return self;
}

- (void)dealloc {
  [_disablePopupsSetting setObserver:nil];
  [super dealloc];
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)loadModel {
  [super loadModel];

  CollectionViewModel* model = self.collectionViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSettings];
  [model addItem:[self blockPopupsItem]
      toSectionWithIdentifier:SectionIdentifierSettings];
  [model addItem:[self translateItem]
      toSectionWithIdentifier:SectionIdentifierSettings];
}

- (CollectionViewItem*)blockPopupsItem {
  _blockPopupsDetailItem.reset([[CollectionViewDetailItem alloc]
      initWithType:ItemTypeSettingsBlockPopups]);
  NSString* subtitle = [_disablePopupsSetting value]
                           ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                           : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _blockPopupsDetailItem.get().text =
      l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
  _blockPopupsDetailItem.get().detailText = subtitle;
  _blockPopupsDetailItem.get().accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  _blockPopupsDetailItem.get().accessibilityTraits |=
      UIAccessibilityTraitButton;
  return _blockPopupsDetailItem;
}

- (CollectionViewItem*)translateItem {
  _translateDetailItem.reset([[CollectionViewDetailItem alloc]
      initWithType:ItemTypeSettingsTranslate]);
  BOOL enabled = browserState_->GetPrefs()->GetBoolean(prefs::kEnableTranslate);
  NSString* subtitle = enabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                               : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _translateDetailItem.get().text =
      l10n_util::GetNSString(IDS_IOS_TRANSLATE_SETTING);
  _translateDetailItem.get().detailText = subtitle;
  _translateDetailItem.get().accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  _translateDetailItem.get().accessibilityTraits |= UIAccessibilityTraitButton;
  return _translateDetailItem;
}

- (ContentSetting)getContentSetting:(ContentSettingsType)settingID {
  return ios::HostContentSettingsMapFactory::GetForBrowserState(browserState_)
      ->GetDefaultContentSetting(settingID, NULL);
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSettingsBlockPopups: {
      base::scoped_nsobject<UIViewController> controller(
          [[BlockPopupsCollectionViewController alloc]
              initWithBrowserState:browserState_]);
      [self.navigationController pushViewController:controller animated:YES];
      break;
    }
    case ItemTypeSettingsTranslate: {
      base::scoped_nsobject<UIViewController> controller(
          [[TranslateCollectionViewController alloc]
              initWithPrefs:browserState_->GetPrefs()]);
      [self.navigationController pushViewController:controller animated:YES];
      break;
    }
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kEnableTranslate) {
    BOOL enabled = browserState_->GetPrefs()->GetBoolean(preferenceName);
    NSString* subtitle = enabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                 : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _translateDetailItem.get().detailText = subtitle;
    [self reconfigureCellsForItems:@[ _translateDetailItem ]
           inSectionWithIdentifier:SectionIdentifierSettings];
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _disablePopupsSetting.get());

  NSString* subtitle = [_disablePopupsSetting value]
                           ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                           : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  // Update the item.
  _blockPopupsDetailItem.get().detailText = subtitle;

  // Update the cell.
  [self reconfigureCellsForItems:@[ _blockPopupsDetailItem ]
         inSectionWithIdentifier:SectionIdentifierSettings];
}

@end
