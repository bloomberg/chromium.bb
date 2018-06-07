// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/clear_browsing_data_consumer.h"
#import "ios/chrome/browser/ui/settings/time_range_selector_collection_view_controller.h"

@class ListModel;

// Clear Browswing Data Section Identifiers.
enum ClearBrowsingDataSectionIdentifier {
  // Section holding types of data that can be cleared.
  SectionDataTypes = kSectionIdentifierEnumZero,
  // Section containing button to clear browsing data.
  SectionClearBrowsingDataButton,
  // Section for informational footnote about user's Google Account data.
  SectionGoogleAccount,
  // Section for footnote about synced data being cleared.
  SectionClearSyncAndSavedSiteData,
  // Section for informational footnote about site settings remaining.
  SectionSavedSiteData,
  // Section containing cell displaying time range to remove data.
  SectionTimeRange,
};

// Clear Browsing Data Item Types.
enum ClearBrowsingDataItemType {
  // Item representing browsing history data.
  DataTypeBrowsingHistory = kItemTypeEnumZero,
  // Item representing cookies and site data.
  DataTypeCookiesSiteData,
  // Items representing cached data.
  DataTypeCache,
  // Items representing saved passwords.
  DataTypeSavedPasswords,
  // Items representing autofill data.
  DataTypeAutofill,
  // Clear data button.
  ClearBrowsingDataButton,
  // Footer noting account will not be signed out.
  FooterGoogleAccount,
  // Footer noting user will not be signed out of chrome and other forms of
  // browsing history will still be available.
  FooterGoogleAccountAndMyActivity,
  // Footer noting site settings will remain.
  FooterSavedSiteData,
  // Footer noting data will be cleared on all devices except for saved
  // settings.
  FooterClearSyncAndSavedSiteData,
  // Item showing time range to remove data and allowing user to edit time
  // range.
  TimeRange,
};

// Manager that serves as the bulk of the logic for
// ClearBrowsingDataConsumer.
@interface ClearBrowsingDataManager
    : NSObject<TimeRangeSelectorCollectionViewControllerDelegate>

// The manager's consumer.
@property(nonatomic, assign) id<ClearBrowsingDataConsumer> consumer;
// Reference to the LinkDelegate for CollectionViewFooterItem.
@property(nonatomic, strong) id<CollectionViewFooterLinkDelegate> linkDelegate;

// Default init method with |browserState| that can't be nil.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Fills |model| with appropriate sections and items.
- (void)loadModel:(ListModel*)model;
// Returns a UIAlertController that has action block to clear data of type
// |dataTypeMaskToRemove|.
- (UIAlertController*)alertControllerWithDataTypesToRemove:
    (BrowsingDataRemoveMask)dataTypeMaskToRemove;
// Get the text to be displayed by a counter from the given |result|
- (NSString*)counterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_MANAGER_H_
