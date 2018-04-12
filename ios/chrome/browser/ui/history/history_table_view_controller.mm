// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_table_view_controller.h"

#include "base/i18n/time_formatting.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/ui/history/history_entries_status_item.h"
#import "ios/chrome/browser/ui/history/history_entries_status_item_delegate.h"
#include "ios/chrome/browser/ui/history/history_entry_inserter.h"
#import "ios/chrome/browser/ui/history/history_entry_item.h"
#import "ios/chrome/browser/ui/history/history_table_view_controller_delegate.h"
#include "ios/chrome/browser/ui/history/history_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/referrer.h"
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
}

@interface HistoryTableViewController ()<HistoryEntriesStatusItemDelegate,
                                         HistoryEntryInserterDelegate> {
  // Closure to request next page of history.
  base::OnceClosure _query_history_continuation;
}

// Object to manage insertion of history entries into the table view model.
@property(nonatomic, strong) HistoryEntryInserter* entryInserter;
// The current query for visible history entries.
@property(nonatomic, copy) NSString* currentQuery;
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
// YES if the table should be filtered by the next received query result.
@property(nonatomic, assign) BOOL filterQueryResult;
// YES if there is a search happening.
@property(nonatomic, assign, getter=isSearching) BOOL searching;
@end

@implementation HistoryTableViewController
@synthesize browserState = _browserState;
@synthesize currentQuery = _currentQuery;
@synthesize delegate = _delegate;
@synthesize empty = _empty;
@synthesize entryInserter = _entryInserter;
@synthesize filterQueryResult = _filterQueryResult;
@synthesize finishedLoading = _finishedLoading;
@synthesize historyService = _historyService;
@synthesize loader = _loader;
@synthesize loading = _loading;
@synthesize searching = _searching;
@synthesize shouldShowNoticeAboutOtherFormsOfBrowsingHistory =
    _shouldShowNoticeAboutOtherFormsOfBrowsingHistory;

#pragma mark - ViewController Lifecycle.

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];

  // TableView configuration
  self.tableView.estimatedRowHeight = 56;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionHeaderHeight = 56;
  self.tableView.sectionFooterHeight = 0.0;
  self.tableView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
  self.clearsSelectionOnViewWillAppear = NO;
}

// TODO(crbug.com/805190): These methods are supposed to be public, though we
// should consider using a delegate instead.
#pragma mark - Public Interface

- (void)setSearching:(BOOL)searching {
  _searching = searching;
  [self updateEntriesStatusMessage];
}

- (BOOL)hasSelectedEntries {
  return self.tableView.indexPathsForSelectedRows.count;
}

- (void)deleteSelectedItemsFromHistory {
  // TODO(crbug.com/805190): Migrate.
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];
  // Add initial info section as header.
  [self.tableViewModel
      addSectionWithIdentifier:kEntriesStatusSectionIdentifier];
  _entryInserter =
      [[HistoryEntryInserter alloc] initWithModel:self.tableViewModel];
  _entryInserter.delegate = self;
  _empty = YES;
  [self showHistoryMatchingQuery:nil];
}

#pragma mark - Protocols

#pragma mark HistoryConsumer

- (void)historyQueryWasCompletedWithResults:
            (const std::vector<BrowsingHistoryService::HistoryEntry>&)results
                           queryResultsInfo:
                               (const BrowsingHistoryService::QueryResultsInfo&)
                                   queryResultsInfo
                        continuationClosure:
                            (base::OnceClosure)continuationClosure {
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
  if (results.empty() && self.empty) {
    [self updateEntriesStatusMessage];
    [self.delegate historyTableViewControllerDidChangeEntries];
    return;
  }

  self.finishedLoading = queryResultsInfo.reached_beginning;
  self.empty = NO;

  // Header section should be updated outside of batch updates, otherwise
  // loading indicator removal will not be observed.
  [self updateEntriesStatusMessage];

  NSMutableArray* resultsItems = [NSMutableArray array];
  NSString* searchQuery =
      [base::SysUTF16ToNSString(queryResultsInfo.search_text) copy];

  void (^tableUpdates)(void) = ^{
    // There should always be at least a header section present.
    DCHECK([[self tableViewModel] numberOfSections]);
    for (const BrowsingHistoryService::HistoryEntry& entry : results) {
      HistoryEntryItem* item =
          [[HistoryEntryItem alloc] initWithType:ItemTypeHistoryEntry];
      item.text = [history::FormattedTitle(entry.title, entry.url) copy];
      item.detailText =
          [base::SysUTF8ToNSString(entry.url.GetOrigin().spec()) copy];
      item.timeText = [base::SysUTF16ToNSString(
          base::TimeFormatTimeOfDay(entry.time)) copy];
      item.URL = entry.url;
      item.timestamp = entry.time;
      [resultsItems addObject:item];
    }
    [self.delegate historyTableViewControllerDidChangeEntries];
    if (([self isSearching] && [searchQuery length] > 0 &&
         [self.currentQuery isEqualToString:searchQuery]) ||
        self.filterQueryResult) {
      // If in search mode, filter out entries that are not part of the
      // search result.
      [self filterForHistoryEntries:resultsItems];
      NSArray* deletedIndexPaths = self.tableView.indexPathsForSelectedRows;
      [self deleteItemsFromtableViewModelWithIndex:deletedIndexPaths];
      self.filterQueryResult = NO;
    }
    // Wait to insert until after the deletions are done, this is needed
    // because performBatchUpdates processes deletion indexes first, and
    // then inserts.
    for (HistoryEntryItem* item in resultsItems) {
      [self.entryInserter insertHistoryEntryItem:item];
    }
  };

  // If iOS11+ use performBatchUpdates: instead of beginUpdates/endUpdates.
  if (@available(iOS 11, *)) {
    [self.tableView performBatchUpdates:tableUpdates
                             completion:^(BOOL) {
                               [self updateTableViewAfterDeletingEntries];
                             }];
  } else {
    [self.tableView beginUpdates];
    tableUpdates();
    [self updateTableViewAfterDeletingEntries];
    [self.tableView endUpdates];
  }
}

- (void)showNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)shouldShowNotice {
  self.shouldShowNoticeAboutOtherFormsOfBrowsingHistory = shouldShowNotice;
  // Update the history entries status message if there is no query in progress.
  if (!self.isLoading) {
    [self updateEntriesStatusMessage];
  }
}

- (void)historyWasDeleted {
  // If history has been deleted, reload history filtering for the current
  // results. This only observes local changes to history, i.e. removing
  // history via the clear browsing data page.
  self.filterQueryResult = YES;
  [self showHistoryMatchingQuery:nil];
}

#pragma mark HistoryEntriesStatusItemDelegate

- (void)historyEntriesStatusItem:(HistoryEntriesStatusItem*)item
               didRequestOpenURL:(const GURL&)URL {
  // TODO(crbug.com/805190): Migrate.
}

#pragma mark HistoryEntryInserterDelegate

- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
    didInsertItemAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView insertRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
}

- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
     didInsertSectionAtIndex:(NSInteger)sectionIndex {
  [self.tableView insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                withRowAnimation:UITableViewRowAnimationNone];
}

- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
     didRemoveSectionAtIndex:(NSInteger)sectionIndex {
  [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                withRowAnimation:UITableViewRowAnimationNone];
}

#pragma mark HistoryEntryItemDelegate
// TODO(crbug.com/805190): Migrate once we decide how to handle favicons and the
// a11y callback on HistoryEntryItem.

#pragma mark HistoryTableUpdaterDelegate

- (void)showHistoryMatchingQuery:(NSString*)query {
  self.finishedLoading = NO;
  self.currentQuery = query;
  [self fetchHistoryForQuery:query continuation:false];
}
- (void)deleteSelectedItems {
  // TODO(crbug.com/805190): Migrate.
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if (section ==
      [self.tableViewModel
          sectionForSectionIdentifier:kEntriesStatusSectionIdentifier])
    return 0;
  return UITableViewAutomaticDimension;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if (self.isEditing) {
    [self.delegate historyTableViewControllerDidChangeEntrySelection];
  } else {
    HistoryEntryItem* item = base::mac::ObjCCastStrict<HistoryEntryItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    [self openURL:item.URL];
    if (self.isSearching) {
      base::RecordAction(
          base::UserMetricsAction("HistoryPage_SearchResultClick"));
    } else {
      base::RecordAction(base::UserMetricsAction("HistoryPage_EntryLinkClick"));
    }
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
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
    NSInteger lastSection = [self.tableViewModel numberOfSections] - 1;
    NSInteger lastItemIndex =
        [self.tableViewModel numberOfItemsInSection:lastSection] - 1;
    if (lastSection == 0 || lastItemIndex < 0) {
      return;
    }

    [self fetchHistoryForQuery:_currentQuery continuation:true];
  }
}

#pragma mark - Private methods

// Fetches history for search text |query|. If |query| is nil or the empty
// string, all history is fetched. If continuation is false, then the most
// recent results are fetched, otherwise the results more recent than the
// previous query will be returned.
- (void)fetchHistoryForQuery:(NSString*)query continuation:(BOOL)continuation {
  self.loading = YES;
  // Add loading indicator if no items are shown.
  if (self.empty && !self.isSearching) {
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
    self.historyService->QueryHistory(queryString, options);
  }
}

// Updates various elements after history items have been deleted from the
// TableView.
- (void)updateTableViewAfterDeletingEntries {
  // TODO(crbug.com/805190): Migrate.
}

// Updates header section to provide relevant information about the currently
// displayed history entries.
- (void)updateEntriesStatusMessage {
  // TODO(crbug.com/805190): Migrate.
}

// Removes selected items from the tableView, but does not delete them from
// browser history.
- (void)removeSelectedItemsFromTableView {
  // TODO(crbug.com/805190): Migrate.
}

// Deletes all items in the tableView which indexes are included in indexArray,
// needs to be run inside a performBatchUpdates block.
- (void)deleteItemsFromtableViewModelWithIndex:(NSArray*)indexArray {
  // TODO(crbug.com/805190): Migrate.
}

// Selects all items in the tableView that are not included in entries.
- (void)filterForHistoryEntries:(NSArray*)entries {
  // TODO(crbug.com/805190): Migrate.
}

// Adds loading indicator to the top of the history tableView, if one is not
// already present.
- (void)addLoadingIndicator {
  // TODO(crbug.com/805190): Migrate.
}

#pragma mark Context Menu

// Displays context menu on cell pressed with gestureRecognizer.
- (void)displayContextMenuInvokedByGestureRecognizer:
    (UILongPressGestureRecognizer*)gestureRecognizer {
  // TODO(crbug.com/805190): Migrate.
}

// Opens URL in a new non-incognito tab and dismisses the history view.
- (void)openURLInNewTab:(const GURL&)URL {
  // TODO(crbug.com/805190): Migrate.
}

// Opens URL in a new incognito tab and dismisses the history view.
- (void)openURLInNewIncognitoTab:(const GURL&)URL {
  // TODO(crbug.com/805190): Migrate.
}

#pragma mark Helper Methods

// Opens URL in the current tab and dismisses the history view.
- (void)openURL:(const GURL&)URL {
  // Make a copy to make sure the referenced URL doesn't change while we're
  // opening it.
  GURL copiedURL(URL);
  new_tab_page_uma::RecordAction(_browserState,
                                 new_tab_page_uma::ACTION_OPENED_HISTORY_ENTRY);
  [self.delegate dismissHistoryWithCompletion:^{
    [self.loader loadURL:copiedURL
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
        rendererInitiated:NO];
  }];
}

@end
