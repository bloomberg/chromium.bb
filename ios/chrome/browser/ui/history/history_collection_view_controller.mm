// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_collection_view_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include <memory>

#include "base/callback.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/activity_indicator_cell.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"
#include "ios/chrome/browser/ui/history/history_entries_status_item.h"
#include "ios/chrome/browser/ui/history/history_entry_inserter.h"
#import "ios/chrome/browser/ui/history/history_entry_item.h"
#include "ios/chrome/browser/ui/history/history_util.h"
#include "ios/chrome/browser/ui/history/ios_browsing_history_driver.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MDCActivityIndicator.h"
#import "ios/third_party/material_components_ios/src/components/Collections/src/MaterialCollections.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/web/public/referrer.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using history::BrowsingHistoryService;

namespace {
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHistoryEntry = kItemTypeEnumZero,
  ItemTypeEntriesStatus,
  ItemTypeActivityIndicator,
};
// Section identifier for the header (sync information) section.
const NSInteger kEntriesStatusSectionIdentifier = kSectionIdentifierEnumZero;
// Maximum number of entries to retrieve in a single query to history service.
const int kMaxFetchCount = 100;
// Horizontal inset for item separators.
const CGFloat kSeparatorInset = 10;
}

@interface HistoryCollectionViewController ()<HistoryEntriesStatusItemDelegate,
                                              HistoryEntryInserterDelegate,
                                              HistoryEntryItemDelegate,
                                              BrowsingHistoryDriverDelegate> {
  // Abstraction to communicate with HistoryService and WebHistoryService.
  std::unique_ptr<BrowsingHistoryService> _browsingHistoryService;
  // Provides dependencies and funnels callbacks from BrowsingHistoryService.
  std::unique_ptr<IOSBrowsingHistoryDriver> _browsingHistoryDriver;
  // The main browser state. Not owned by HistoryCollectionViewController.
  ios::ChromeBrowserState* _browserState;
  // Backing ivar for delegate property.
  __weak id<HistoryCollectionViewControllerDelegate> _delegate;
  // Backing ivar for URLLoader property.
  __weak id<UrlLoader> _URLLoader;
  // Closure to request next page of history.
  base::OnceClosure _query_history_continuation;
}

// Object to manage insertion of history entries into the collection view model.
@property(nonatomic, strong) HistoryEntryInserter* entryInserter;
// Delegate for the history collection view.
@property(nonatomic, weak, readonly) id<HistoryCollectionViewControllerDelegate>
    delegate;
// UrlLoader for navigating to history entries.
@property(nonatomic, weak, readonly) id<UrlLoader> URLLoader;
// The current query for visible history entries.
@property(nonatomic, copy) NSString* currentQuery;
// Coordinator for displaying context menus for history entries.
@property(nonatomic, strong) ContextMenuCoordinator* contextMenuCoordinator;
// YES if there are no results to show.
@property(nonatomic, assign) BOOL empty;
// YES if the history panel should show a notice about additional forms of
// browsing history.
@property(nonatomic, assign)
    BOOL shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
// YES if there is an outstanding history query.
@property(nonatomic, assign, getter=isLoading) BOOL loading;
// YES if there are no more history entries to load.
@property(nonatomic, assign, getter=hasFinishedLoading) BOOL finishedLoading;
// YES if the collection should be filtered by the next received query result.
@property(nonatomic, assign) BOOL filterQueryResult;

// Fetches history for search text |query|. If |query| is nil or the empty
// string, all history is fetched. If continuation is false, then the most
// recent results are fetched, otherwise the results more recent than the
// previous query will be returned.
- (void)fetchHistoryForQuery:(NSString*)query continuation:(BOOL)continuation;
// Updates header section to provide relevant information about the currently
// displayed history entries.
- (void)updateEntriesStatusMessage;
// Removes selected items from the visible collection, but does not delete them
// from browser history.
- (void)removeSelectedItemsFromCollection;
// Removes all items in the collection that are not included in entries.
- (void)filterForHistoryEntries:(NSArray*)entries;
// Adds loading indicator to the top of the history collection, if one is not
// already present.
- (void)addLoadingIndicator;
// Displays context menu on cell pressed with gestureRecognizer.
- (void)displayContextMenuInvokedByGestureRecognizer:
    (UILongPressGestureRecognizer*)gestureRecognizer;
// Opens URL in the current tab and dismisses the history view.
- (void)openURL:(const GURL&)URL;
// Opens URL in a new non-incognito tab and dismisses the history view.
- (void)openURLInNewTab:(const GURL&)URL;
// Opens URL in a new incognito tab and dismisses the history view.
- (void)openURLInNewIncognitoTab:(const GURL&)URL;
@end

@implementation HistoryCollectionViewController

@synthesize searching = _searching;
@synthesize entryInserter = _entryInserter;
@synthesize currentQuery = _currentQuery;
@synthesize contextMenuCoordinator = _contextMenuCoordinator;
@synthesize empty = _empty;
@synthesize shouldShowNoticeAboutOtherFormsOfBrowsingHistory =
    _shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
@synthesize loading = _loading;
@synthesize finishedLoading = _finishedLoading;
@synthesize filterQueryResult = _filterQueryResult;

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState
                      delegate:(id<HistoryCollectionViewControllerDelegate>)
                                   delegate {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleDefault];
  if (self) {
    _browsingHistoryDriver =
        std::make_unique<IOSBrowsingHistoryDriver>(browserState, self);
    _browsingHistoryService = std::make_unique<BrowsingHistoryService>(
        _browsingHistoryDriver.get(),
        ios::HistoryServiceFactory::GetForBrowserState(
            browserState, ServiceAccessType::EXPLICIT_ACCESS),
        IOSChromeProfileSyncServiceFactory::GetForBrowserState(browserState));
    _browserState = browserState;
    _delegate = delegate;
    _URLLoader = loader;
    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    // Consider moving the other complex code out of the initializer as well.
    [self loadModel];
    // Add initial info section as header.
    [self.collectionViewModel
        addSectionWithIdentifier:kEntriesStatusSectionIdentifier];
    _entryInserter =
        [[HistoryEntryInserter alloc] initWithModel:self.collectionViewModel];
    _entryInserter.delegate = self;
    _empty = YES;
    [self showHistoryMatchingQuery:nil];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.styler.cellLayoutType = MDCCollectionViewCellLayoutTypeList;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorInset, 0, kSeparatorInset);
  self.styler.allowsItemInlay = NO;

  self.clearsSelectionOnViewWillAppear = NO;
  self.collectionView.keyboardDismissMode =
      UIScrollViewKeyboardDismissModeOnDrag;

  UILongPressGestureRecognizer* longPressRecognizer = [
      [UILongPressGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(displayContextMenuInvokedByGestureRecognizer:)];
  [self.collectionView addGestureRecognizer:longPressRecognizer];
}

- (BOOL)isEditing {
  return self.editor.isEditing;
}

- (void)setEditing:(BOOL)editing {
  [self.editor setEditing:editing animated:YES];
}

- (void)setSearching:(BOOL)searching {
  _searching = searching;
  [self updateEntriesStatusMessage];
}

- (BOOL)hasSelectedEntries {
  return self.collectionView.indexPathsForSelectedItems.count;
}

- (void)showHistoryMatchingQuery:(NSString*)query {
  self.finishedLoading = NO;
  self.currentQuery = query;
  [self fetchHistoryForQuery:query continuation:false];
}

- (void)deleteSelectedItemsFromHistory {
  NSArray* deletedIndexPaths = self.collectionView.indexPathsForSelectedItems;
  std::vector<BrowsingHistoryService::HistoryEntry> entries;
  for (NSIndexPath* indexPath in deletedIndexPaths) {
    HistoryEntryItem* object = base::mac::ObjCCastStrict<HistoryEntryItem>(
        [self.collectionViewModel itemAtIndexPath:indexPath]);
    BrowsingHistoryService::HistoryEntry entry;
    entry.url = object.URL;
    entry.all_timestamps.insert(object.timestamp.ToInternalValue());
    entries.push_back(entry);
  }
  _browsingHistoryService->RemoveVisits(entries);
  [self removeSelectedItemsFromCollection];
}

- (id<HistoryCollectionViewControllerDelegate>)delegate {
  return _delegate;
}

- (id<UrlLoader>)URLLoader {
  return _URLLoader;
}

#pragma mark - MDCollectionViewController

// TODO(crbug.com/653547): Remove this once the MDC adds an option for
// preventing the infobar from showing.
- (void)updateFooterInfoBarIfNecessary {
  // No-op. This prevents the default infobar from showing.
}

#pragma mark - HistoryEntriesStatusItemDelegate

- (void)historyEntriesStatusItem:(HistoryEntriesStatusItem*)item
               didRequestOpenURL:(const GURL&)URL {
  [self openURLInNewTab:URL];
}

#pragma mark - HistoryEntryInserterDelegate

- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
    didInsertItemAtIndexPath:(NSIndexPath*)indexPath {
  [self.collectionView insertItemsAtIndexPaths:@[ indexPath ]];
}

- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
     didInsertSectionAtIndex:(NSInteger)sectionIndex {
  [self.collectionView
      insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]];
}

- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
     didRemoveSectionAtIndex:(NSInteger)sectionIndex {
  [self.collectionView
      deleteSections:[NSIndexSet indexSetWithIndex:sectionIndex]];
}

#pragma mark - HistoryEntryItemDelegate

- (void)historyEntryItemDidRequestOpen:(HistoryEntryItem*)item {
  [self openURL:item.URL];
}

- (void)historyEntryItemDidRequestDelete:(HistoryEntryItem*)item {
  NSInteger sectionIdentifier =
      [self.entryInserter sectionIdentifierForTimestamp:item.timestamp];
  if ([self.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier] &&
      [self.collectionViewModel hasItem:item
                inSectionWithIdentifier:sectionIdentifier]) {
    NSIndexPath* indexPath = [self.collectionViewModel indexPathForItem:item];
    [self.collectionView
        selectItemAtIndexPath:indexPath
                     animated:NO
               scrollPosition:UICollectionViewScrollPositionNone];
    [self deleteSelectedItemsFromHistory];
  }
}

- (void)historyEntryItemDidRequestCopy:(HistoryEntryItem*)item {
  StoreURLInPasteboard(item.URL);
}

- (void)historyEntryItemDidRequestOpenInNewTab:(HistoryEntryItem*)item {
  [self openURLInNewTab:item.URL];
}

- (void)historyEntryItemDidRequestOpenInNewIncognitoTab:
    (HistoryEntryItem*)item {
  [self openURLInNewIncognitoTab:item.URL];
}

- (void)historyEntryItemShouldUpdateView:(HistoryEntryItem*)item {
  NSInteger sectionIdentifier =
      [self.entryInserter sectionIdentifierForTimestamp:item.timestamp];
  // If the item is still in the model, reconfigure it.
  if ([self.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier] &&
      [self.collectionViewModel hasItem:item
                inSectionWithIdentifier:sectionIdentifier]) {
    [self reconfigureCellsForItems:@[ item ]];
  }
}

#pragma mark - BrowsingHistoryDriverDelegate

- (void)onQueryCompleteWithResults:
            (const std::vector<BrowsingHistoryService::HistoryEntry>&)results
                  queryResultsInfo:
                      (const BrowsingHistoryService::QueryResultsInfo&)
                          queryResultsInfo
               continuationClosure:(base::OnceClosure)continuationClosure {
  self.loading = NO;
  _query_history_continuation = std::move(continuationClosure);

  // If history sync is enabled and there hasn't been a response from synced
  // history, try fetching again.
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(_browserState);
  if (syncSetupService->IsSyncEnabled() &&
      syncSetupService->IsDataTypeActive(syncer::HISTORY_DELETE_DIRECTIVES) &&
      queryResultsInfo.sync_timed_out) {
    [self showHistoryMatchingQuery:_currentQuery];
    return;
  }

  // If there are no results and no URLs have been loaded, report that no
  // history entries were found.
  if (results.empty() && self.isEmpty) {
    [self updateEntriesStatusMessage];
    [self.delegate historyCollectionViewControllerDidChangeEntries:self];
    return;
  }

  self.finishedLoading = queryResultsInfo.reached_beginning;
  self.empty = NO;

  // Header section should be updated outside of batch updates, otherwise
  // loading indicator removal will not be observed.
  [self updateEntriesStatusMessage];

  NSMutableArray* filterResults = [NSMutableArray array];
  NSString* searchQuery =
      [base::SysUTF16ToNSString(queryResultsInfo.search_text) copy];
  [self.collectionView performBatchUpdates:^{
    // There should always be at least a header section present.
    DCHECK([[self collectionViewModel] numberOfSections]);
    for (const BrowsingHistoryService::HistoryEntry& entry : results) {
      HistoryEntryItem* item =
          [[HistoryEntryItem alloc] initWithType:ItemTypeHistoryEntry
                                    historyEntry:entry
                                    browserState:_browserState
                                        delegate:self];
      [self.entryInserter insertHistoryEntryItem:item];
      if ([self isSearching] || self.filterQueryResult) {
        [filterResults addObject:item];
      }
    }
    [self.delegate historyCollectionViewControllerDidChangeEntries:self];
  }
      completion:^(BOOL) {
        if (([self isSearching] && [searchQuery length] > 0 &&
             [self.currentQuery isEqualToString:searchQuery]) ||
            self.filterQueryResult) {
          // If in search mode, filter out entries that are not part of the
          // search result.
          [self filterForHistoryEntries:filterResults];
          self.filterQueryResult = NO;
        }
      }];
}

- (void)shouldShowNoticeAboutOtherFormsOfBrowsingHistory:
    (BOOL)shouldShowNotice {
  self.shouldShowNoticeAboutOtherFormsOfBrowsingHistory = shouldShowNotice;
  // Update the history entries status message if there is no query in progress.
  if (!self.isLoading) {
    [self updateEntriesStatusMessage];
  }
}

- (void)didObserverHistoryDeletion {
  // If history has been deleted, reload history filtering for the current
  // results. This only observes local changes to history, i.e. removing
  // history via the clear browsing data page.
  self.filterQueryResult = YES;
  [self showHistoryMatchingQuery:nil];
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(UICollectionView*)collectionView {
  return YES;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canEditItemAtIndexPath:(NSIndexPath*)indexPath {
  // All items except those in the header section may be edited.
  return indexPath.section;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canSelectItemDuringEditingAtIndexPath:(NSIndexPath*)indexPath {
  // All items except those in the header section may be edited.
  return indexPath.section;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  // Display the entries status section (always the first section) without any
  // background image or shadowing.
  return !indexPath.section;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  return [indexPath isEqual:[NSIndexPath indexPathForItem:0 inSection:0]];
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section) {
    return MDCCellDefaultTwoLineHeight;
  } else {
    DCHECK([indexPath isEqual:[NSIndexPath indexPathForItem:0 inSection:0]]);
    // Configure size for loading indicator and entries status cells.
    CollectionViewItem* item =
        [self.collectionViewModel itemAtIndexPath:indexPath];
    if ([item isKindOfClass:[CollectionViewTextItem class]]) {
      return MDCCellDefaultOneLineHeight;
    }
    CGFloat height = [[item cellClass]
        cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                           forItem:item];
    return height;
  }
}

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  return section ? MDCCollectionViewCellStyleCard
                 : MDCCollectionViewCellStyleDefault;
}

#pragma mark - UICollectionViewDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  // The first section is not selectable.
  return indexPath.section && [super collectionView:collectionView
                                  shouldSelectItemAtIndexPath:indexPath];
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  if (self.isEditing) {
    [self.delegate historyCollectionViewControllerDidChangeEntrySelection:self];
  } else {
    HistoryEntryItem* item = base::mac::ObjCCastStrict<HistoryEntryItem>(
        [self.collectionViewModel itemAtIndexPath:indexPath]);
    [self openURL:item.URL];
    if (self.isSearching) {
      base::RecordAction(
          base::UserMetricsAction("HistoryPage_SearchResultClick"));
    } else {
      base::RecordAction(base::UserMetricsAction("HistoryPage_EntryLinkClick"));
    }
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    didDeselectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didDeselectItemAtIndexPath:indexPath];
  [self.delegate historyCollectionViewControllerDidChangeEntrySelection:self];
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndDisplayingCell:(UICollectionViewCell*)cell
      forItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell isKindOfClass:[ActivityIndicatorCell class]]) {
    [[base::mac::ObjCCast<ActivityIndicatorCell>(cell) activityIndicator]
        stopAnimating];
  }
}
#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];
  // Adjust header shadow.
  [self.delegate historyCollectionViewController:self
                               didScrollToOffset:scrollView.contentOffset];

  if (self.hasFinishedLoading)
    return;

  CGFloat insetHeight =
      scrollView.contentInset.top + scrollView.contentInset.bottom;
  CGFloat contentViewHeight = scrollView.bounds.size.height - insetHeight;
  CGFloat contentHeight = scrollView.contentSize.height;
  CGFloat contentOffset = scrollView.contentOffset.y;
  CGFloat buffer = contentViewHeight;
  // If the scroll view is approaching the end of loaded history, try to fetch
  // more history. Do so when the content offset is greater than the content
  // height minus the view height, minus a buffer to start the fetch early.
  if (contentOffset > (contentHeight - contentViewHeight) - buffer &&
      !self.isLoading) {
    // If at end, try to grab more history.
    NSInteger lastSection = [self.collectionViewModel numberOfSections] - 1;
    NSInteger lastItemIndex =
        [self.collectionViewModel numberOfItemsInSection:lastSection] - 1;
    if (lastSection == 0 || lastItemIndex < 0) {
      return;
    }

    [self fetchHistoryForQuery:_currentQuery continuation:true];
  }
}

#pragma mark - Private methods

- (void)fetchHistoryForQuery:(NSString*)query continuation:(BOOL)continuation {
  self.loading = YES;
  // Add loading indicator if no items are shown.
  if (self.isEmpty && !self.isSearching) {
    [self addLoadingIndicator];
  }

  if (continuation) {
    DCHECK(_query_history_continuation);
    std::move(_query_history_continuation).Run();
  } else {
    _query_history_continuation.Reset();

    BOOL fetchAllHistory = !query || [query isEqualToString:@""];
    base::string16 queryString =
        fetchAllHistory ? base::string16() : base::SysNSStringToUTF16(query);
    history::QueryOptions options;
    options.duplicate_policy =
        fetchAllHistory ? history::QueryOptions::REMOVE_DUPLICATES_PER_DAY
                        : history::QueryOptions::REMOVE_ALL_DUPLICATES;
    options.max_count = kMaxFetchCount;
    options.matching_algorithm =
        query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH;
    _browsingHistoryService->QueryHistory(queryString, options);
  }
}

- (void)updateEntriesStatusMessage {
  CollectionViewItem* entriesStatusItem = nil;
  if (self.isEmpty) {
    CollectionViewTextItem* noResultsItem =
        [[CollectionViewTextItem alloc] initWithType:ItemTypeEntriesStatus];
    noResultsItem.text =
        self.isSearching ? l10n_util::GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS)
                         : l10n_util::GetNSString(IDS_HISTORY_NO_RESULTS);
    entriesStatusItem = noResultsItem;
  } else {
    HistoryEntriesStatusItem* historyEntriesStatusItem =
        [[HistoryEntriesStatusItem alloc] initWithType:ItemTypeEntriesStatus];
    historyEntriesStatusItem.delegate = self;
    historyEntriesStatusItem.hidden = self.isSearching;
    historyEntriesStatusItem.showsOtherBrowsingDataNotice =
        _shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
    entriesStatusItem = historyEntriesStatusItem;
  }
  // Replace the item in the first section, which is always present.
  NSArray* items = [self.collectionViewModel
      itemsInSectionWithIdentifier:kEntriesStatusSectionIdentifier];
  if ([items count]) {
    // There should only ever be one item in this section.
    DCHECK([items count] == 1);
    // Only update if the item has changed.
    if ([items[0] isEqual:entriesStatusItem]) {
      return;
    }
  }
  [self.collectionView performBatchUpdates:^{
    NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0 inSection:0];
    if ([items count]) {
      [self.collectionViewModel
                 removeItemWithType:[self.collectionViewModel
                                        itemTypeForIndexPath:indexPath]
          fromSectionWithIdentifier:kEntriesStatusSectionIdentifier];
      [self.collectionView deleteItemsAtIndexPaths:@[ indexPath ]];
    }
    [self.collectionViewModel addItem:entriesStatusItem
              toSectionWithIdentifier:kEntriesStatusSectionIdentifier];
    [self.collectionView insertItemsAtIndexPaths:@[ indexPath ]];
  }
                                completion:nil];
}

- (void)removeSelectedItemsFromCollection {
  NSArray* deletedIndexPaths = self.collectionView.indexPathsForSelectedItems;
  [self.collectionView performBatchUpdates:^{
    [self collectionView:self.collectionView
        willDeleteItemsAtIndexPaths:deletedIndexPaths];
    [self.collectionView deleteItemsAtIndexPaths:deletedIndexPaths];

    // Remove any empty sections, except the header section.
    for (int section = self.collectionView.numberOfSections - 1; section > 0;
         --section) {
      if (![self.collectionViewModel numberOfItemsInSection:section]) {
        [self.entryInserter removeSection:section];
      }
    }
  }
      completion:^(BOOL) {
        // If only the header section remains, there are no history entries.
        if ([self.collectionViewModel numberOfSections] == 1) {
          self.empty = YES;
        }
        [self updateEntriesStatusMessage];
        [self.delegate historyCollectionViewControllerDidChangeEntries:self];
      }];
}

- (void)filterForHistoryEntries:(NSArray*)entries {
  self.collectionView.allowsMultipleSelection = YES;
  for (int section = 1; section < [self.collectionViewModel numberOfSections];
       ++section) {
    NSInteger sectionIdentifier =
        [self.collectionViewModel sectionIdentifierForSection:section];
    if ([self.collectionViewModel
            hasSectionForSectionIdentifier:sectionIdentifier]) {
      NSArray* items = [self.collectionViewModel
          itemsInSectionWithIdentifier:sectionIdentifier];
      for (id item in items) {
        HistoryEntryItem* historyItem =
            base::mac::ObjCCastStrict<HistoryEntryItem>(item);
        if (![entries containsObject:historyItem]) {
          NSIndexPath* indexPath =
              [self.collectionViewModel indexPathForItem:historyItem];
          [self.collectionView
              selectItemAtIndexPath:indexPath
                           animated:NO
                     scrollPosition:UICollectionViewScrollPositionNone];
        }
      }
    }
  }
  [self removeSelectedItemsFromCollection];
}

- (void)addLoadingIndicator {
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0 inSection:0];
  if ([self.collectionViewModel hasItemAtIndexPath:indexPath] &&
      [self.collectionViewModel itemTypeForIndexPath:indexPath] ==
          ItemTypeActivityIndicator) {
    // Do not add indicator a second time.
    return;
  }

  [self.collectionView performBatchUpdates:^{
    if ([self.collectionViewModel hasItemAtIndexPath:indexPath]) {
      [self.collectionViewModel
                 removeItemWithType:[self.collectionViewModel
                                        itemTypeForIndexPath:indexPath]
          fromSectionWithIdentifier:kSectionIdentifierEnumZero];
      [self.collectionView deleteItemsAtIndexPaths:@[ indexPath ]];
    }
    CollectionViewItem* loadingIndicatorItem =
        [[CollectionViewItem alloc] initWithType:ItemTypeActivityIndicator];
    loadingIndicatorItem.cellClass = [ActivityIndicatorCell class];
    [self.collectionViewModel addItem:loadingIndicatorItem
              toSectionWithIdentifier:kEntriesStatusSectionIdentifier];
    [self.collectionView insertItemsAtIndexPaths:@[ indexPath ]];
  }
                                completion:nil];
}

#pragma mark Context Menu

- (void)displayContextMenuInvokedByGestureRecognizer:
    (UILongPressGestureRecognizer*)gestureRecognizer {
  if (gestureRecognizer.numberOfTouches != 1 || self.editing ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }

  CGPoint touchLocation =
      [gestureRecognizer locationOfTouch:0 inView:self.collectionView];
  NSIndexPath* touchedItemIndexPath =
      [self.collectionView indexPathForItemAtPoint:touchLocation];
  // If there's no index path, or the index path is for the header item, do not
  // display a contextual menu.
  if (!touchedItemIndexPath ||
      [touchedItemIndexPath
          isEqual:[NSIndexPath indexPathForItem:0 inSection:0]])
    return;

  HistoryEntryItem* entry = base::mac::ObjCCastStrict<HistoryEntryItem>(
      [self.collectionViewModel itemAtIndexPath:touchedItemIndexPath]);

  __weak HistoryCollectionViewController* weakSelf = self;
  web::ContextMenuParams params;
  params.location = touchLocation;
  params.view.reset(self.collectionView);
  NSString* menuTitle =
      base::SysUTF16ToNSString(url_formatter::FormatUrl(entry.URL));
  params.menu_title.reset([menuTitle copy]);

  // Present sheet/popover using controller that is added to view hierarchy.
  UIViewController* topController = [params.view window].rootViewController;
  while (topController.presentedViewController)
    topController = topController.presentedViewController;

  self.contextMenuCoordinator =
      [[ContextMenuCoordinator alloc] initWithBaseViewController:topController
                                                          params:params];

  // TODO(crbug.com/606503): Refactor context menu creation code to be shared
  // with BrowserViewController.
  // Add "Open in New Tab" option.
  NSString* openInNewTabTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  ProceduralBlock openInNewTabAction = ^{
    [weakSelf openURLInNewTab:entry.URL];
  };
  [self.contextMenuCoordinator addItemWithTitle:openInNewTabTitle
                                         action:openInNewTabAction];

  // Add "Open in New Incognito Tab" option.
  NSString* openInNewIncognitoTabTitle = l10n_util::GetNSStringWithFixup(
      IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
  ProceduralBlock openInNewIncognitoTabAction = ^{
    [weakSelf openURLInNewIncognitoTab:entry.URL];
  };
  [self.contextMenuCoordinator addItemWithTitle:openInNewIncognitoTabTitle
                                         action:openInNewIncognitoTabAction];

  // Add "Copy URL" option.
  NSString* copyURLTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_COPY);
  ProceduralBlock copyURLAction = ^{
    StoreURLInPasteboard(entry.URL);
  };
  [self.contextMenuCoordinator addItemWithTitle:copyURLTitle
                                         action:copyURLAction];
  [self.parentViewController.view endEditing:YES];
  [self.contextMenuCoordinator start];
}

- (void)openURL:(const GURL&)URL {
  GURL copiedURL(URL);
  [self.delegate historyCollectionViewController:self
                       shouldCloseWithCompletion:^{
                         [self.URLLoader
                                       loadURL:copiedURL
                                      referrer:web::Referrer()
                                    transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
                             rendererInitiated:NO];
                       }];
}

- (void)openURLInNewTab:(const GURL&)URL {
  GURL copiedURL(URL);
  [self.delegate historyCollectionViewController:self
                       shouldCloseWithCompletion:^{
                         [self.URLLoader webPageOrderedOpen:copiedURL
                                                   referrer:web::Referrer()
                                                inIncognito:NO
                                               inBackground:NO
                                                   appendTo:kLastTab];
                       }];
}

- (void)openURLInNewIncognitoTab:(const GURL&)URL {
  GURL copiedURL(URL);
  [self.delegate historyCollectionViewController:self
                       shouldCloseWithCompletion:^{
                         [self.URLLoader webPageOrderedOpen:copiedURL
                                                   referrer:web::Referrer()
                                                inIncognito:YES
                                               inBackground:NO
                                                   appendTo:kLastTab];
                       }];
}

@end
