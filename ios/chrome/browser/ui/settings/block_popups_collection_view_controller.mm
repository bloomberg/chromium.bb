// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/block_popups_collection_view_controller.h"

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMainSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierExceptions,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMainSwitch = kItemTypeEnumZero,
  ItemTypeHeader,
  ItemTypeException,
};

}  // namespace

@interface BlockPopupsCollectionViewController ()<BooleanObserver> {
  ios::ChromeBrowserState* _browserState;  // weak

  // List of url patterns that are allowed to display popups.
  base::ListValue _exceptions;

  // The observable boolean that binds to the "Disable Popups" setting state.
  base::scoped_nsobject<ContentSettingBackedBoolean> _disablePopupsSetting;

  // The item related to the switch for the "Disable Popups" setting.
  base::scoped_nsobject<CollectionViewSwitchItem> _blockPopupsItem;
}

// Fetch the urls that can display popups and add them to |_exceptions|.
- (void)populateExceptionsList;

// Returns YES if popups are currently blocked by default, NO otherwise.
- (BOOL)popupsCurrentlyBlocked;

@end

@implementation BlockPopupsCollectionViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    _browserState = browserState;
    HostContentSettingsMap* settingsMap =
        ios::HostContentSettingsMapFactory::GetForBrowserState(_browserState);
    _disablePopupsSetting.reset([[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:CONTENT_SETTINGS_TYPE_POPUPS
                              inverted:YES]);
    [_disablePopupsSetting setObserver:self];
    self.title = l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
    self.collectionViewAccessibilityIdentifier =
        @"block_popups_settings_view_controller";

    [self populateExceptionsList];
    [self updateEditButton];
    [self loadModel];
  }
  return self;
}

- (void)dealloc {
  [_disablePopupsSetting setObserver:nil];
  [super dealloc];
}

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];

  CollectionViewModel* model = self.collectionViewModel;

  // Block popups switch.
  [model addSectionWithIdentifier:SectionIdentifierMainSwitch];

  _blockPopupsItem.reset(
      [[CollectionViewSwitchItem alloc] initWithType:ItemTypeMainSwitch]);
  _blockPopupsItem.get().text = l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
  _blockPopupsItem.get().on = [_disablePopupsSetting value];
  _blockPopupsItem.get().accessibilityIdentifier =
      @"blockPopupsContentView_switch";
  [model addItem:_blockPopupsItem
      toSectionWithIdentifier:SectionIdentifierMainSwitch];

  if ([self popupsCurrentlyBlocked] && _exceptions.GetSize()) {
    [self populateExceptionsItems];
  }
}

- (BOOL)shouldShowEditButton {
  return [self popupsCurrentlyBlocked];
}

- (BOOL)editButtonEnabled {
  return _exceptions.GetSize() > 0;
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(UICollectionView*)collectionView {
  return YES;
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  if ([self.collectionViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeMainSwitch) {
    CollectionViewSwitchCell* switchCell =
        base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(blockPopupsSwitchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  UICollectionReusableView* view = [super collectionView:collectionView
                       viewForSupplementaryElementOfKind:kind
                                             atIndexPath:indexPath];
  MDCCollectionViewTextCell* textCell =
      base::mac::ObjCCast<MDCCollectionViewTextCell>(view);
  if (textCell) {
    textCell.textLabel.textColor = [[MDCPalette greyPalette] tint500];
  }
  return view;
};

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    canEditItemAtIndexPath:(NSIndexPath*)indexPath {
  // Any item in SectionIdentifierExceptions is editable.
  return [self.collectionViewModel
             sectionIdentifierForSection:indexPath.section] ==
         SectionIdentifierExceptions;
}

- (void)collectionView:(UICollectionView*)collectionView
    willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  for (NSIndexPath* indexPath in indexPaths) {
    size_t urlIndex = indexPath.item;
    std::string urlToRemove;
    _exceptions.GetString(urlIndex, &urlToRemove);

    // Remove the exception for the site by resetting its popup setting to the
    // default.
    ios::HostContentSettingsMapFactory::GetForBrowserState(_browserState)
        ->SetContentSettingCustomScope(
            ContentSettingsPattern::FromString(urlToRemove),
            ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_POPUPS,
            std::string(), CONTENT_SETTING_DEFAULT);

    // Remove the site from |_exceptions|.
    _exceptions.Remove(urlIndex, NULL);
  }

  // Must call super at the end of the child implementation.
  [super collectionView:collectionView willDeleteItemsAtIndexPaths:indexPaths];
}

- (void)collectionView:(UICollectionView*)collectionView
    didDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  // The only editable section is the block popups exceptions section.
  if ([self.collectionViewModel
          hasSectionForSectionIdentifier:SectionIdentifierExceptions]) {
    NSInteger exceptionsSectionIndex = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierExceptions];
    if ([collectionView numberOfItemsInSection:exceptionsSectionIndex] == 0) {
      base::WeakNSObject<BlockPopupsCollectionViewController> weakSelf(self);
      [self.collectionView performBatchUpdates:^{
        base::scoped_nsobject<BlockPopupsCollectionViewController> strongSelf(
            [weakSelf retain]);
        if (!strongSelf) {
          return;
        }
        NSInteger section = [strongSelf.get().collectionViewModel
            sectionForSectionIdentifier:SectionIdentifierExceptions];
        [strongSelf.get().collectionViewModel
            removeSectionWithIdentifier:SectionIdentifierExceptions];
        [strongSelf.get().collectionView
            deleteSections:[NSIndexSet indexSetWithIndex:section]];
      }
                                    completion:nil];
    }
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeMainSwitch:
      return YES;
    default:
      return NO;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _disablePopupsSetting.get());

  // Update the item.
  _blockPopupsItem.get().on = [_disablePopupsSetting value];

  // Update the cell.
  [self reconfigureCellsForItems:@[ _blockPopupsItem ]
         inSectionWithIdentifier:SectionIdentifierMainSwitch];

  // Update the rest of the UI.
  [self.editor setEditing:NO];
  [self updateEditButton];
  [self layoutSections:[_disablePopupsSetting value]];
}

#pragma mark - Actions

- (void)blockPopupsSwitchChanged:(UISwitch*)switchView {
  // Update the setting.
  [_disablePopupsSetting setValue:switchView.on];

  // Update the item.
  _blockPopupsItem.get().on = [_disablePopupsSetting value];

  // Update the rest of the UI.
  [self.editor setEditing:NO];
  [self updateEditButton];
  [self layoutSections:switchView.on];
}

#pragma mark - Private

- (BOOL)popupsCurrentlyBlocked {
  return [_disablePopupsSetting value];
}

- (void)populateExceptionsList {
  // The body of this method was mostly copied from
  // chrome/browser/ui/webui/options/content_settings_handler.cc and simplified
  // to only deal with urls/patterns that allow popups.
  ContentSettingsForOneType entries;
  ios::HostContentSettingsMapFactory::GetForBrowserState(_browserState)
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_POPUPS, std::string(),
                              &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    // Skip default settings from extensions and policy, and the default content
    // settings; all of them will affect the default setting UI.
    if (entries[i].primary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].secondary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].source != "preference") {
      continue;
    }
    // The content settings UI does not support secondary content settings
    // pattern yet. For content settings set through the content settings UI the
    // secondary pattern is by default a wildcard pattern. Hence users are not
    // able to modify content settings with a secondary pattern other than the
    // wildcard pattern. So only show settings that the user is able to modify.
    if (entries[i].secondary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].setting == CONTENT_SETTING_ALLOW) {
      _exceptions.Append(
          new base::StringValue(entries[i].primary_pattern.ToString()));
    } else {
      LOG(ERROR) << "Secondary content settings patterns are not "
                 << "supported by the content settings UI";
    }
  }
}

- (void)populateExceptionsItems {
  CollectionViewModel* model = self.collectionViewModel;
  [model addSectionWithIdentifier:SectionIdentifierExceptions];

  CollectionViewTextItem* header = [
      [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader] autorelease];
  header.text = l10n_util::GetNSString(IDS_IOS_POPUPS_ALLOWED);
  [model setHeader:header forSectionWithIdentifier:SectionIdentifierExceptions];

  for (size_t i = 0; i < _exceptions.GetSize(); ++i) {
    std::string allowed_url;
    _exceptions.GetString(i, &allowed_url);
    CollectionViewTextItem* item = [[[CollectionViewTextItem alloc]
        initWithType:ItemTypeException] autorelease];
    item.text = base::SysUTF8ToNSString(allowed_url);
    [model addItem:item toSectionWithIdentifier:SectionIdentifierExceptions];
  }
}

- (void)layoutSections:(BOOL)blockPopupsIsOn {
  BOOL hasExceptions = _exceptions.GetSize();
  BOOL exceptionsListShown = [self.collectionViewModel
      hasSectionForSectionIdentifier:SectionIdentifierExceptions];

  if (blockPopupsIsOn && !exceptionsListShown && hasExceptions) {
    // Animate in the list of exceptions. Animation looks much better if the
    // section is added at once, rather than row-by-row as each object is added.
    base::WeakNSObject<BlockPopupsCollectionViewController> weakSelf(self);
    [self.collectionView performBatchUpdates:^{
      base::scoped_nsobject<BlockPopupsCollectionViewController> strongSelf(
          [weakSelf retain]);
      if (!strongSelf)
        return;
      [strongSelf populateExceptionsItems];
      NSUInteger index = [[strongSelf collectionViewModel]
          sectionForSectionIdentifier:SectionIdentifierExceptions];
      [[strongSelf collectionView]
          insertSections:[NSIndexSet indexSetWithIndex:index]];
    }
                                  completion:nil];
  } else if (!blockPopupsIsOn && exceptionsListShown) {
    // Make sure the exception section is not shown.
    base::WeakNSObject<BlockPopupsCollectionViewController> weakSelf(self);
    [self.collectionView performBatchUpdates:^{
      base::scoped_nsobject<BlockPopupsCollectionViewController> strongSelf(
          [weakSelf retain]);
      if (!strongSelf)
        return;
      NSUInteger index = [[strongSelf collectionViewModel]
          sectionForSectionIdentifier:SectionIdentifierExceptions];
      [[strongSelf collectionViewModel]
          removeSectionWithIdentifier:SectionIdentifierExceptions];
      [[strongSelf collectionView]
          deleteSections:[NSIndexSet indexSetWithIndex:index]];
    }
                                  completion:nil];
  }
}

@end
