// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"

#include <memory>
#include <string>

#include "base/ios/ios_util.h"
#include "base/logging.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/google/core/browser/google_util.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"
#include "ios/chrome/browser/browsing_data/ios_browsing_data_counter_factory.h"
#include "ios/chrome/browser/browsing_data/ios_chrome_browsing_data_remover.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/clear_browsing_data_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/time_range_selector_collection_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kClearBrowsingDataCollectionViewId =
    @"kClearBrowsingDataCollectionViewId";
NSString* const kClearBrowsingHistoryCellId = @"kClearBrowsingHistoryCellId";
NSString* const kClearCookiesCellId = @"kClearCookiesCellId";
NSString* const kClearCacheCellId = @"kClearCacheCellId";
NSString* const kClearSavedPasswordsCellId = @"kClearSavedPasswordsCellId";
NSString* const kClearAutofillCellId = @"kClearAutofillCellId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierDataTypes = kSectionIdentifierEnumZero,
  SectionIdentifierClearBrowsingDataButton,
  SectionIdentifierGoogleAccount,
  SectionIdentifierClearSyncAndSavedSiteData,
  SectionIdentifierSavedSiteData,
  SectionIdentifierTimeRange,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeDataTypeBrowsingHistory = kItemTypeEnumZero,
  ItemTypeDataTypeCookiesSiteData,
  ItemTypeDataTypeCache,
  ItemTypeDataTypeSavedPasswords,
  ItemTypeDataTypeAutofill,
  ItemTypeClearBrowsingDataButton,
  ItemTypeFooterGoogleAccount,
  ItemTypeFooterGoogleAccountAndMyActivity,
  ItemTypeFooterSavedSiteData,
  ItemTypeFooterClearSyncAndSavedSiteData,
  ItemTypeTimeRange,
};

const int kMaxTimesHistoryNoticeShown = 1;

}  // namespace

// Collection view item identifying a clear browsing data content view.
@interface ClearDataItem : CollectionViewTextItem {
  // Data volume counter associated with the item.
  std::unique_ptr<BrowsingDataCounterWrapper> _counter;
}

// Mask of the data to be cleared.
@property(nonatomic, assign) int dataTypeMask;

// Pref name associated with the item.
@property(nonatomic, assign) const char* prefName;

// Sets the counter associated with the data type represented by the item.
- (void)setCounter:(std::unique_ptr<BrowsingDataCounterWrapper>)counter;

// Checks if the item has a counter.
- (BOOL)hasCounter;

// Restarts the counter.
- (void)restartCounter;

@end

@implementation ClearDataItem

@synthesize dataTypeMask = _dataTypeMask;
@synthesize prefName = _prefName;

- (void)setCounter:(std::unique_ptr<BrowsingDataCounterWrapper>)counter {
  _counter = std::move(counter);
}

- (BOOL)hasCounter {
  return !!_counter;
}

- (void)restartCounter {
  if (_counter)
    _counter->RestartCounter();
}

@end

@interface ClearBrowsingDataCollectionViewController ()<
    TimeRangeSelectorCollectionViewControllerDelegate> {
  ios::ChromeBrowserState* _browserState;  // weak

  browsing_data::TimePeriod _timePeriod;

  IOSChromeBrowsingDataRemover::CallbackSubscription _callbackSubscription;
}

@property(nonatomic, assign)
    BOOL shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
@property(nonatomic, assign)
    BOOL shouldPopupDialogAboutOtherFormsOfBrowsingHistory;

// Displays an action sheet to the user confirming the clearing of user data. If
// the clearing is confirmed, clears the data.
// Always returns YES to ensure that the collection view cell is deselected.
- (BOOL)alertAndClearData;

// Clears the data stored for |dataTypeMask|.
- (void)clearDataForDataTypes:(int)dataTypeMask;

// Returns the accessibility identifier for the cell corresponding to
// |itemType|.
- (NSString*)getAccessibilityIdentifierFromItemType:(NSInteger)itemType;

// Restarts the counters for data types specified in the mask.
- (void)restartCounters:(int)data_mask;

@end

@implementation ClearBrowsingDataCollectionViewController

@synthesize shouldShowNoticeAboutOtherFormsOfBrowsingHistory =
    _shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
@synthesize shouldPopupDialogAboutOtherFormsOfBrowsingHistory =
    _shouldPopupDialogAboutOtherFormsOfBrowsingHistory;

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    _browserState = browserState;
    if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
      int prefValue = browserState->GetPrefs()->GetInteger(
          browsing_data::prefs::kDeleteTimePeriod);
      prefValue = MAX(0, prefValue);
      const int maxValue =
          static_cast<int>(browsing_data::TimePeriod::TIME_PERIOD_LAST);
      if (prefValue > maxValue) {
        prefValue = maxValue;
      }
      _timePeriod = static_cast<browsing_data::TimePeriod>(prefValue);
    } else {
      _timePeriod = browsing_data::TimePeriod::ALL_TIME;
    }

    self.title = l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
    self.collectionViewAccessibilityIdentifier =
        kClearBrowsingDataCollectionViewId;

    if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
      __weak ClearBrowsingDataCollectionViewController* weakSelf = self;
      void (^dataClearedCallback)(
          const IOSChromeBrowsingDataRemover::NotificationDetails&) =
          ^(const IOSChromeBrowsingDataRemover::NotificationDetails& details) {
            ClearBrowsingDataCollectionViewController* strongSelf = weakSelf;
            [strongSelf restartCounters:details.removal_mask];
          };
      _callbackSubscription =
          IOSChromeBrowsingDataRemover::RegisterOnBrowsingDataRemovedCallback(
              base::BindBlockArc(dataClearedCallback));
    }

    [self loadModel];
    [self restartCounters:IOSChromeBrowsingDataRemover::REMOVE_ALL];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  SigninManager* signinManager =
      ios::SigninManagerFactory::GetForBrowserState(_browserState);
  if (!signinManager->IsAuthenticated()) {
    return;
  }

  browser_sync::ProfileSyncService* syncService =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(_browserState);
  history::WebHistoryService* historyService =
      ios::WebHistoryServiceFactory::GetForBrowserState(_browserState);

  __weak ClearBrowsingDataCollectionViewController* weakSelf = self;
  browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      syncService, historyService, base::BindBlockArc(^(bool shouldShowNotice) {
        ClearBrowsingDataCollectionViewController* strongSelf = weakSelf;
        [strongSelf setShouldShowNoticeAboutOtherFormsOfBrowsingHistory:
                        shouldShowNotice];
      }));

  browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
      syncService, historyService, GetChannel(),
      base::BindBlockArc(^(bool shouldShowPopup) {
        ClearBrowsingDataCollectionViewController* strongSelf = weakSelf;
        [strongSelf setShouldPopupDialogAboutOtherFormsOfBrowsingHistory:
                        shouldShowPopup];
      }));
}

#pragma mark CollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  // Time range section.
  if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierTimeRange];
    [model addItem:[self timeRangeItem]
        toSectionWithIdentifier:SectionIdentifierTimeRange];
  }

  // Data types section.
  [model addSectionWithIdentifier:SectionIdentifierDataTypes];
  int clearBrowsingHistoryMask =
      IOSChromeBrowsingDataRemover::REMOVE_HISTORY |
      IOSChromeBrowsingDataRemover::REMOVE_GOOGLE_APP_LAUNCHER_DATA;
  CollectionViewItem* browsingHistoryItem =
      [self clearDataItemWithType:ItemTypeDataTypeBrowsingHistory
                          titleID:IDS_IOS_CLEAR_BROWSING_HISTORY
                             mask:clearBrowsingHistoryMask
                         prefName:browsing_data::prefs::kDeleteBrowsingHistory];
  [model addItem:browsingHistoryItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  // This data type doesn't currently have an associated counter, but displays
  // an explanatory text instead, when the new UI is enabled.
  ClearDataItem* cookiesSiteDataItem =
      [self clearDataItemWithType:ItemTypeDataTypeCookiesSiteData
                          titleID:IDS_IOS_CLEAR_COOKIES
                             mask:IOSChromeBrowsingDataRemover::REMOVE_SITE_DATA
                         prefName:browsing_data::prefs::kDeleteCookies];
  if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
    if (_browserState->GetPrefs()->GetBoolean(
            browsing_data::prefs::kDeleteCookies)) {
      cookiesSiteDataItem.detailText =
          l10n_util::GetNSString(IDS_DEL_COOKIES_COUNTER);
    }
  }
  [model addItem:cookiesSiteDataItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  ClearDataItem* cacheItem =
      [self clearDataItemWithType:ItemTypeDataTypeCache
                          titleID:IDS_IOS_CLEAR_CACHE
                             mask:IOSChromeBrowsingDataRemover::REMOVE_CACHE
                         prefName:browsing_data::prefs::kDeleteCache];
  [model addItem:cacheItem toSectionWithIdentifier:SectionIdentifierDataTypes];

  ClearDataItem* savedPasswordsItem =
      [self clearDataItemWithType:ItemTypeDataTypeSavedPasswords
                          titleID:IDS_IOS_CLEAR_SAVED_PASSWORDS
                             mask:IOSChromeBrowsingDataRemover::REMOVE_PASSWORDS
                         prefName:browsing_data::prefs::kDeletePasswords];
  [model addItem:savedPasswordsItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  ClearDataItem* autofillItem =
      [self clearDataItemWithType:ItemTypeDataTypeAutofill
                          titleID:IDS_IOS_CLEAR_AUTOFILL
                             mask:IOSChromeBrowsingDataRemover::REMOVE_FORM_DATA
                         prefName:browsing_data::prefs::kDeleteFormData];
  [model addItem:autofillItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  // Clear Browsing Data button.
  [model addSectionWithIdentifier:SectionIdentifierClearBrowsingDataButton];
  CollectionViewTextItem* clearButtonItem = [[CollectionViewTextItem alloc]
      initWithType:ItemTypeClearBrowsingDataButton];
  clearButtonItem.text = l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON);
  clearButtonItem.accessibilityTraits |= UIAccessibilityTraitButton;
  clearButtonItem.textColor = [[MDCPalette cr_redPalette] tint500];
  [model addItem:clearButtonItem
      toSectionWithIdentifier:SectionIdentifierClearBrowsingDataButton];

  // Google Account footer.
  SigninManager* signinManager =
      ios::SigninManagerFactory::GetForBrowserState(_browserState);
  if (signinManager->IsAuthenticated()) {
    // TODO(crbug.com/650424): Footer items must currently go into a separate
    // section, to work around a drawing bug in MDC.
    [model addSectionWithIdentifier:SectionIdentifierGoogleAccount];
    [model addItem:[self footerForGoogleAccountSectionItem]
        toSectionWithIdentifier:SectionIdentifierGoogleAccount];
  }

  browser_sync::ProfileSyncService* syncService =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(_browserState);
  if (syncService && syncService->IsSyncActive()) {
    // TODO(crbug.com/650424): Footer items must currently go into a separate
    // section, to work around a drawing bug in MDC.
    [model addSectionWithIdentifier:SectionIdentifierClearSyncAndSavedSiteData];
    [model addItem:[self footerClearSyncAndSavedSiteDataItem]
        toSectionWithIdentifier:SectionIdentifierClearSyncAndSavedSiteData];
  } else {
    // TODO(crbug.com/650424): Footer items must currently go into a separate
    // section, to work around a drawing bug in MDC.
    [model addSectionWithIdentifier:SectionIdentifierSavedSiteData];
    [model addItem:[self footerSavedSiteDataItem]
        toSectionWithIdentifier:SectionIdentifierSavedSiteData];
  }
}

#pragma mark Items

- (ClearDataItem*)clearDataItemWithType:(ItemType)itemType
                                titleID:(int)titleMessageID
                                   mask:(int)mask
                               prefName:(const char*)prefName {
  PrefService* prefs = _browserState->GetPrefs();
  ClearDataItem* clearDataItem = [[ClearDataItem alloc] initWithType:itemType];
  clearDataItem.text = l10n_util::GetNSString(titleMessageID);
  if (prefs->GetBoolean(prefName)) {
    clearDataItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
  }
  clearDataItem.dataTypeMask = mask;
  clearDataItem.prefName = prefName;
  clearDataItem.accessibilityIdentifier =
      [self getAccessibilityIdentifierFromItemType:itemType];

  __weak ClearBrowsingDataCollectionViewController* weakSelf = self;
  void (^updateUICallback)(const browsing_data::BrowsingDataCounter::Result&) =
      ^(const browsing_data::BrowsingDataCounter::Result& result) {
        ClearBrowsingDataCollectionViewController* strongSelf = weakSelf;
        NSString* counterText = [strongSelf getCounterTextFromResult:result];
        [strongSelf updateCounter:itemType detailText:counterText];
      };

  [clearDataItem setCounter:BrowsingDataCounterWrapper::CreateCounterWrapper(
                                prefName, _browserState, prefs,
                                base::BindBlockArc(updateUICallback))];
  return clearDataItem;
}

- (void)updateCounter:(NSInteger)itemType detailText:(NSString*)detailText {
  NSIndexPath* indexPath = [self.collectionViewModel
      indexPathForItemType:itemType
         sectionIdentifier:SectionIdentifierDataTypes];

  CollectionViewModel* model = self.collectionViewModel;
  if (!model) {
    return;
  }
  ClearDataItem* clearDataItem = base::mac::ObjCCastStrict<ClearDataItem>(
      [model itemAtIndexPath:indexPath]);
  // Because there is no counter for cookies, an explanatory text is displayed.
  if (![clearDataItem hasCounter] &&
      itemType != ItemTypeDataTypeCookiesSiteData) {
    return;
  }
  clearDataItem.detailText = detailText;
  [self reconfigureCellsForItems:@[ clearDataItem ]];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (CollectionViewItem*)footerForGoogleAccountSectionItem {
  return _shouldShowNoticeAboutOtherFormsOfBrowsingHistory
             ? [self footerGoogleAccountAndMyActivityItem]
             : [self footerGoogleAccountItem];
}

- (CollectionViewItem*)footerGoogleAccountItem {
  CollectionViewFooterItem* footerItem = [[CollectionViewFooterItem alloc]
      initWithType:ItemTypeFooterGoogleAccount];
  footerItem.text =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT);
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetBrandedImageProvider()
                       ->GetClearBrowsingDataAccountActivityImage();
  footerItem.image = image;
  return footerItem;
}

- (CollectionViewItem*)footerGoogleAccountAndMyActivityItem {
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetBrandedImageProvider()
                       ->GetClearBrowsingDataAccountActivityImage();
  return [self
      footerItemWithType:ItemTypeFooterGoogleAccountAndMyActivity
                 titleID:IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT_AND_HISTORY
                     URL:kClearBrowsingDataMyActivityUrlInFooterURL
                   image:image];
}

- (CollectionViewItem*)footerSavedSiteDataItem {
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetBrandedImageProvider()
                       ->GetClearBrowsingDataSiteDataImage();
  return [self
      footerItemWithType:ItemTypeFooterSavedSiteData
                 titleID:IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_SAVED_SITE_DATA
                     URL:kClearBrowsingDataLearnMoreURL
                   image:image];
}

- (CollectionViewItem*)footerClearSyncAndSavedSiteDataItem {
  UIImage* infoIcon = [ChromeIcon infoIcon];
  UIImage* image = TintImage(infoIcon, [[MDCPalette greyPalette] tint500]);
  return [self
      footerItemWithType:ItemTypeFooterClearSyncAndSavedSiteData
                 titleID:
                     IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_CLEAR_SYNC_AND_SAVED_SITE_DATA
                     URL:kClearBrowsingDataLearnMoreURL
                   image:image];
}

- (CollectionViewItem*)footerItemWithType:(ItemType)itemType
                                  titleID:(int)titleMessageID
                                      URL:(const char[])URL
                                    image:(UIImage*)image {
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:itemType];
  footerItem.text = l10n_util::GetNSString(titleMessageID);
  footerItem.linkURL = google_util::AppendGoogleLocaleParam(
      GURL(URL), GetApplicationContext()->GetApplicationLocale());
  footerItem.linkDelegate = self;
  footerItem.image = image;
  return footerItem;
}

- (CollectionViewItem*)timeRangeItem {
  CollectionViewDetailItem* timeRangeItem =
      [[CollectionViewDetailItem alloc] initWithType:ItemTypeTimeRange];
  timeRangeItem.text = l10n_util::GetNSString(
      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE);
  NSString* detailText = [TimeRangeSelectorCollectionViewController
      timePeriodLabelForPrefs:_browserState->GetPrefs()];
  DCHECK(detailText);
  timeRangeItem.detailText = detailText;
  timeRangeItem.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  timeRangeItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return timeRangeItem;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypeTimeRange: {
      UIViewController* controller =
          [[TimeRangeSelectorCollectionViewController alloc]
              initWithPrefs:_browserState->GetPrefs()
                   delegate:self];
      [self.navigationController pushViewController:controller animated:YES];
      break;
    }
    case ItemTypeDataTypeBrowsingHistory:
    case ItemTypeDataTypeCookiesSiteData:
    case ItemTypeDataTypeCache:
    case ItemTypeDataTypeSavedPasswords:
    case ItemTypeDataTypeAutofill: {
      // Toggle the checkmark.
      // TODO(crbug.com/631486): Custom checkmark animation to be implemented.
      ClearDataItem* clearDataItem = base::mac::ObjCCastStrict<ClearDataItem>(
          [self.collectionViewModel itemAtIndexPath:indexPath]);
      if (clearDataItem.accessoryType == MDCCollectionViewCellAccessoryNone) {
        clearDataItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
        if (experimental_flags::IsNewClearBrowsingDataUIEnabled() &&
            itemType == ItemTypeDataTypeCookiesSiteData) {
          [self updateCounter:itemType
                   detailText:l10n_util::GetNSString(IDS_DEL_COOKIES_COUNTER)];
        }
        _browserState->GetPrefs()->SetBoolean(clearDataItem.prefName, true);
      } else {
        clearDataItem.accessoryType = MDCCollectionViewCellAccessoryNone;
        _browserState->GetPrefs()->SetBoolean(clearDataItem.prefName, false);
        if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
          // Hide counter text.
          [self updateCounter:itemType detailText:nil];
        }
      }
      [self reconfigureCellsForItems:@[ clearDataItem ]];
      break;
    }
    case ItemTypeClearBrowsingDataButton:
      [self alertAndClearData];
      break;
    default:
      break;
  }
}

#pragma mark Properties

- (void)setShouldShowNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)showNotice {
  _shouldShowNoticeAboutOtherFormsOfBrowsingHistory = showNotice;
  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.HistoryNoticeShownInFooterWhenUpdated",
      _shouldShowNoticeAboutOtherFormsOfBrowsingHistory);

  // Update the account footer if the model was already loaded.
  CollectionViewModel* model = self.collectionViewModel;
  if (!model) {
    return;
  }
  SigninManager* signinManager =
      ios::SigninManagerFactory::GetForBrowserState(_browserState);
  if (!signinManager->IsAuthenticated()) {
    return;
  }

  CollectionViewItem* footerItem = [self footerForGoogleAccountSectionItem];
  // TODO(crbug.com/650424): Simplify with setFooter:inSection: when the bug in
  // MDC is fixed.
  // Remove the footer if there is one in that section.
  if ([model hasSectionForSectionIdentifier:SectionIdentifierGoogleAccount]) {
    if ([model hasItemForItemType:ItemTypeFooterGoogleAccount
                sectionIdentifier:SectionIdentifierGoogleAccount]) {
      [model removeItemWithType:ItemTypeFooterGoogleAccount
          fromSectionWithIdentifier:SectionIdentifierGoogleAccount];
    } else {
      [model removeItemWithType:ItemTypeFooterGoogleAccountAndMyActivity
          fromSectionWithIdentifier:SectionIdentifierGoogleAccount];
    }
  }
  // Add the new footer.
  [model addItem:footerItem
      toSectionWithIdentifier:SectionIdentifierGoogleAccount];
  [self reconfigureCellsForItems:@[ footerItem ]];

  // Relayout the cells to adapt to the new contents height.
  [self.collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark Clear browsing data

- (BOOL)alertAndClearData {
  int dataTypeMaskToRemove = 0;
  NSArray* dataTypeItems = [self.collectionViewModel
      itemsInSectionWithIdentifier:SectionIdentifierDataTypes];
  for (ClearDataItem* dataTypeItem in dataTypeItems) {
    DCHECK([dataTypeItem isKindOfClass:[ClearDataItem class]]);
    if (dataTypeItem.accessoryType == MDCCollectionViewCellAccessoryCheckmark) {
      dataTypeMaskToRemove |= dataTypeItem.dataTypeMask;
    }
  }
  if (dataTypeMaskToRemove == 0) {
    // Nothing to clear (no data types selected).
    return YES;
  }
  __weak ClearBrowsingDataCollectionViewController* weakSelf = self;
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  UIAlertAction* clearDataAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)
                style:UIAlertActionStyleDestructive
              handler:^(UIAlertAction* action) {
                [weakSelf clearDataForDataTypes:dataTypeMaskToRemove];
              }];
  clearDataAction.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_CONFIRM_CLEAR_BUTTON);
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:clearDataAction];
  [alertController addAction:cancelAction];
  [self presentViewController:alertController animated:YES completion:nil];
  return YES;
}

- (void)clearDataForDataTypes:(int)dataTypeMask {
  DCHECK(dataTypeMask);
  ClearBrowsingDataCommand* command =
      [[ClearBrowsingDataCommand alloc] initWithBrowserState:_browserState
                                                        mask:dataTypeMask
                                                  timePeriod:_timePeriod];
  [self chromeExecuteCommand:command];

  if (!!(dataTypeMask && IOSChromeBrowsingDataRemover::REMOVE_HISTORY)) {
    [self showBrowsingHistoryRemovedDialog];
  }
}

- (void)showBrowsingHistoryRemovedDialog {
  PrefService* prefs = _browserState->GetPrefs();
  int noticeShownTimes = prefs->GetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

  // When the deletion is complete, we might show an additional dialog with
  // a notice about other forms of browsing history. This is the case if
  const bool showDialog =
      // 1. The dialog is relevant for the user.
      _shouldPopupDialogAboutOtherFormsOfBrowsingHistory &&
      // 2. The notice has been shown less than |kMaxTimesHistoryNoticeShown|.
      noticeShownTimes < kMaxTimesHistoryNoticeShown;
  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing", showDialog);
  if (!showDialog) {
    return;
  }

  // Increment the preference.
  prefs->SetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
      noticeShownTimes + 1);

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_DESCRIPTION);

  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak ClearBrowsingDataCollectionViewController* weakSelf = self;
  UIAlertAction* openMyActivityAction = [UIAlertAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OPEN_HISTORY_BUTTON)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                [weakSelf openMyActivityLink];
              }];

  UIAlertAction* okAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OK_BUTTON)
                style:UIAlertActionStyleCancel
              handler:nil];
  [alertController addAction:openMyActivityAction];
  [alertController addAction:okAction];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)openMyActivityLink {
  OpenUrlCommand* openMyActivityCommand =
      [[OpenUrlCommand alloc] initWithURLFromChrome:GURL(kGoogleMyAccountURL)];
  openMyActivityCommand.tag = IDC_CLOSE_SETTINGS_AND_OPEN_URL;
  [self chromeExecuteCommand:openMyActivityCommand];
}

- (NSString*)getAccessibilityIdentifierFromItemType:(NSInteger)itemType {
  switch (itemType) {
    case ItemTypeDataTypeBrowsingHistory:
      return kClearBrowsingHistoryCellId;
    case ItemTypeDataTypeCookiesSiteData:
      return kClearCookiesCellId;
    case ItemTypeDataTypeCache:
      return kClearCacheCellId;
    case ItemTypeDataTypeSavedPasswords:
      return kClearSavedPasswordsCellId;
    case ItemTypeDataTypeAutofill:
      return kClearAutofillCellId;
    default: {
      NOTREACHED();
      return nil;
    }
  }
}

- (void)restartCounters:(int)data_mask {
  CollectionViewModel* model = self.collectionViewModel;
  if (!model)
    return;

  if (data_mask &
      (IOSChromeBrowsingDataRemover::REMOVE_HISTORY |
       IOSChromeBrowsingDataRemover::REMOVE_GOOGLE_APP_LAUNCHER_DATA)) {
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeDataTypeBrowsingHistory
           sectionIdentifier:SectionIdentifierDataTypes];
    ClearDataItem* historyItem = base::mac::ObjCCastStrict<ClearDataItem>(
        [model itemAtIndexPath:indexPath]);
    [historyItem restartCounter];
  }

  if (data_mask & IOSChromeBrowsingDataRemover::REMOVE_PASSWORDS) {
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeDataTypeSavedPasswords
           sectionIdentifier:SectionIdentifierDataTypes];
    ClearDataItem* passwordsItem = base::mac::ObjCCastStrict<ClearDataItem>(
        [model itemAtIndexPath:indexPath]);
    [passwordsItem restartCounter];
  }

  if (data_mask & IOSChromeBrowsingDataRemover::REMOVE_FORM_DATA) {
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeDataTypeAutofill
           sectionIdentifier:SectionIdentifierDataTypes];
    ClearDataItem* autofillItem = base::mac::ObjCCastStrict<ClearDataItem>(
        [model itemAtIndexPath:indexPath]);
    [autofillItem restartCounter];
  }
}

- (NSString*)getCounterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result {
  std::string prefName = result.source()->GetPrefName();
  if (!result.Finished()) {
    // The counter is still counting.
    return l10n_util::GetNSString(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  if (prefName == browsing_data::prefs::kDeleteCache) {
    browsing_data::BrowsingDataCounter::ResultInt cacheSizeBytes =
        static_cast<const browsing_data::BrowsingDataCounter::FinishedResult*>(
            &result)
            ->Value();

    // Three cases: Nonzero result for the entire cache, nonzero result for
    // a subset of cache (i.e. a finite time interval), and almost zero (less
    // than 1 MB). There is no exact information that the cache is empty so that
    // falls into the almost zero case, which is displayed as less than 1 MB.
    // Because of this, the lowest unit that can be used is MB.
    static const int kBytesInAMegabyte = 1 << 20;
    if (cacheSizeBytes >= kBytesInAMegabyte) {
      NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
      formatter.allowedUnits = NSByteCountFormatterUseAll &
                               (~NSByteCountFormatterUseBytes) &
                               (~NSByteCountFormatterUseKB);
      formatter.countStyle = NSByteCountFormatterCountStyleMemory;
      NSString* formattedSize = [formatter stringFromByteCount:cacheSizeBytes];
      return (_timePeriod == browsing_data::TimePeriod::ALL_TIME)
                 ? formattedSize
                 : l10n_util::GetNSStringF(
                       IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                       base::SysNSStringToUTF16(formattedSize));
    }
    return l10n_util::GetNSString(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
  }
  return base::SysUTF16ToNSString(
      browsing_data::GetCounterTextFromResult(&result));
}

#pragma mark MDCCollectionViewStylingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeFooterSavedSiteData:
    case ItemTypeFooterGoogleAccount:
    case ItemTypeFooterGoogleAccountAndMyActivity:
    case ItemTypeFooterClearSyncAndSavedSiteData:
      return YES;
    default:
      return NO;
  }
}

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierGoogleAccount:
    case SectionIdentifierClearSyncAndSavedSiteData:
    case SectionIdentifierSavedSiteData:
      // Display the footer in the default style with no "card" UI and no
      // section padding.
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
    case SectionIdentifierGoogleAccount:
    case SectionIdentifierClearSyncAndSavedSiteData:
    case SectionIdentifierSavedSiteData:
      // Display the Learn More footer without any background image or
      // shadowing.
      return YES;
    default:
      return NO;
  }
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierGoogleAccount:
    case SectionIdentifierClearSyncAndSavedSiteData:
    case SectionIdentifierSavedSiteData: {
      CollectionViewItem* item =
          [self.collectionViewModel itemAtIndexPath:indexPath];
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    }
    case SectionIdentifierDataTypes: {
      ClearDataItem* clearDataItem = base::mac::ObjCCastStrict<ClearDataItem>(
          [self.collectionViewModel itemAtIndexPath:indexPath]);
      return (clearDataItem.detailText.length > 0)
                 ? MDCCellDefaultTwoLineHeight
                 : MDCCellDefaultOneLineHeight;
    }
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

#pragma mark TimeRangeSelectorCollectionViewControllerDelegate

- (void)timeRangeSelectorViewController:
            (TimeRangeSelectorCollectionViewController*)collectionViewController
                    didSelectTimePeriod:(browsing_data::TimePeriod)timePeriod {
  _timePeriod = timePeriod;
}

@end
