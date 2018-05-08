// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_table_view.h"

#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/favicon_server_fetcher_params.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/sync/synced_sessions_bridge.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_empty_background.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_shared_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_waiting_view.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_home_node_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_home_promo_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_signin_promo_cell.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/FlexibleHeader/src/MDCFlexibleHeaderView.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// NetworkTrafficAnnotationTag for fetching favicon from a Google server.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("bookmarks_get_large_icon", R"(
        semantics {
          sender: "Bookmarks"
          description:
            "Sends a request to a Google server to retrieve the favicon bitmap "
            "for a bookmark."
          trigger:
            "A request can be sent if Chrome does not have a favicon for a "
            "bookmark."
          data: "Page URL and desired icon size."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        }
      )");

}  // namespace

using bookmarks::BookmarkNode;

// Used to store a pair of NSIntegers when storing a NSIndexPath in C++
// collections.
using IntegerPair = std::pair<NSInteger, NSInteger>;

@interface BookmarkTableView ()<BookmarkModelBridgeObserver,
                                SyncedSessionsObserver,
                                UIGestureRecognizerDelegate> {
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;

  // Observer to keep track of the signin and syncing status.
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserver;

  // Map of favicon load tasks for each index path. Used to keep track of
  // pending favicon load operations so that they can be cancelled upon cell
  // reuse. Keys are (section, item) pairs of cell index paths.
  std::map<IntegerPair, base::CancelableTaskTracker::TaskId> _faviconLoadTasks;
  // Task tracker used for async favicon loads.
  base::CancelableTaskTracker _faviconTaskTracker;
}

// State used by this table view.
@property(nonatomic, strong) BookmarkHomeSharedState* sharedState;
// The browser state.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The delegate for actions on the table.
@property(nonatomic, weak) id<BookmarkTableViewDelegate> delegate;
// Background shown when there is no bookmarks or folders at the current root
// node.
@property(nonatomic, strong) BookmarkEmptyBackground* emptyTableBackgroundView;
// The loading spinner background which appears when syncing.
@property(nonatomic, strong) BookmarkHomeWaitingView* spinnerView;

@end

@implementation BookmarkTableView

@synthesize browserState = _browserState;
@synthesize delegate = _delegate;
@synthesize emptyTableBackgroundView = _emptyTableBackgroundView;
@synthesize spinnerView = _spinnerView;
@synthesize sharedState = _sharedState;

- (instancetype)initWithSharedState:(BookmarkHomeSharedState*)sharedState
                       browserState:(ios::ChromeBrowserState*)browserState
                           delegate:(id<BookmarkTableViewDelegate>)delegate
                              frame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    DCHECK(sharedState.tableViewDisplayedRootNode);
    _sharedState = sharedState;
    _browserState = browserState;
    _delegate = delegate;

    // Temporary calls to ensure that we're initializing to the right values.
    DCHECK_EQ(self.sharedState.bookmarkModel,
              ios::BookmarkModelFactory::GetForBrowserState(browserState));

    // Set up observers.
    _modelBridge = std::make_unique<bookmarks::BookmarkModelBridge>(
        self, self.sharedState.bookmarkModel);
    _syncedSessionsObserver =
        std::make_unique<synced_sessions::SyncedSessionsObserverBridge>(
            self, _browserState);

    // Populate the model.
    [self computeBookmarkTableViewData];

    // Set promo state before the tableview is created.
    [self promoStateChangedAnimated:NO];

    // Create and setup tableview.
    self.sharedState.tableView =
        [[UITableView alloc] initWithFrame:frame style:UITableViewStylePlain];
    // Remove extra rows.
    self.sharedState.tableView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self addSubview:self.sharedState.tableView];
    [self bringSubviewToFront:self.sharedState.tableView];

    [self showEmptyOrLoadingSpinnerBackgroundIfNeeded];
  }
  return self;
}

- (void)dealloc {
  _faviconTaskTracker.TryCancelAll();
}

#pragma mark - Public

- (void)promoStateChangedAnimated:(BOOL)animated {
  // We show promo cell only on the root view, that is when showing
  // the permanent nodes.
  BOOL promoVisible =
      ((self.sharedState.tableViewDisplayedRootNode ==
        self.sharedState.bookmarkModel->root_node()) &&
       [self.delegate bookmarkTableViewShouldShowPromoCell:self]);

  if (promoVisible == self.sharedState.promoVisible) {
    return;
  }
  self.sharedState.promoVisible = promoVisible;

  SigninPromoViewMediator* mediator = self.delegate.signinPromoViewMediator;
  if (self.sharedState.promoVisible) {
    DCHECK(![self.sharedState.tableViewModel
        hasSectionForSectionIdentifier:BookmarkHomeSectionIdentifierPromo]);
    [self.sharedState.tableViewModel
        insertSectionWithIdentifier:BookmarkHomeSectionIdentifierPromo
                            atIndex:0];
    BookmarkHomePromoItem* item =
        [[BookmarkHomePromoItem alloc] initWithType:BookmarkHomeItemTypePromo];
    item.delegate = self.delegate;
    [self.sharedState.tableViewModel
                        addItem:item
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierPromo];
    [mediator signinPromoViewVisible];
  } else {
    if (![mediator isInvalidClosedOrNeverVisible]) {
      // When the sign-in view is closed, the promo state changes, but
      // -[SigninPromoViewMediator signinPromoViewHidden] should not be called.
      [mediator signinPromoViewHidden];
    }

    DCHECK([self.sharedState.tableViewModel
        hasSectionForSectionIdentifier:BookmarkHomeSectionIdentifierPromo]);
    [self.sharedState.tableViewModel
        removeSectionWithIdentifier:BookmarkHomeSectionIdentifierPromo];
  }
  [self.sharedState.tableView reloadData];
}

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  if (![self.sharedState.tableViewModel
          hasSectionForSectionIdentifier:BookmarkHomeSectionIdentifierPromo]) {
    return;
  }

  NSIndexPath* indexPath = [self.sharedState.tableViewModel
      indexPathForItemType:BookmarkHomeItemTypePromo
         sectionIdentifier:BookmarkHomeSectionIdentifierPromo];
  BookmarkTableSigninPromoCell* signinPromoCell =
      base::mac::ObjCCast<BookmarkTableSigninPromoCell>(
          [self.sharedState.tableView cellForRowAtIndexPath:indexPath]);
  if (!signinPromoCell) {
    return;
  }
  // Should always reconfigure the cell size even if it has to be reloaded,
  // to make sure it has the right size to compute the cell size.
  [configurator configureSigninPromoView:signinPromoCell.signinPromoView];
  if (identityChanged) {
    // The section should be reload to update the cell height.
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:indexPath.section];
    [self.sharedState.tableView reloadSections:indexSet
                              withRowAnimation:UITableViewRowAnimationNone];
  }
}

- (void)addNewFolder {
  [self.sharedState.editingFolderCell stopEdit];
  if (!self.sharedState.tableViewDisplayedRootNode) {
    return;
  }
  self.sharedState.addingNewFolder = YES;
  base::string16 folderTitle = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_GROUP_DEFAULT_NAME));
  self.sharedState.editingFolderNode =
      self.sharedState.bookmarkModel->AddFolder(
          self.sharedState.tableViewDisplayedRootNode,
          self.sharedState.tableViewDisplayedRootNode->child_count(),
          folderTitle);

  BookmarkHomeNodeItem* nodeItem = [[BookmarkHomeNodeItem alloc]
      initWithType:BookmarkHomeItemTypeBookmark
      bookmarkNode:self.sharedState.editingFolderNode];
  [self.sharedState.tableViewModel
                      addItem:nodeItem
      toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];

  // Insert the new folder cell at the end of the table.
  NSIndexPath* newRowIndexPath =
      [self.sharedState.tableViewModel indexPathForItem:nodeItem];
  NSMutableArray* newRowIndexPaths =
      [[NSMutableArray alloc] initWithObjects:newRowIndexPath, nil];
  [self.sharedState.tableView beginUpdates];
  [self.sharedState.tableView
      insertRowsAtIndexPaths:newRowIndexPaths
            withRowAnimation:UITableViewRowAnimationNone];
  [self.sharedState.tableView endUpdates];

  // Scroll to the end of the table
  [self.sharedState.tableView
      scrollToRowAtIndexPath:newRowIndexPath
            atScrollPosition:UITableViewScrollPositionBottom
                    animated:YES];
}

- (std::vector<const bookmarks::BookmarkNode*>)getEditNodesInVector {
  // Create a vector of edit nodes in the same order as the nodes in folder.
  std::vector<const bookmarks::BookmarkNode*> nodes;
  int childCount = self.sharedState.tableViewDisplayedRootNode->child_count();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* node =
        self.sharedState.tableViewDisplayedRootNode->GetChild(i);
    if (self.sharedState.editNodes.find(node) !=
        self.sharedState.editNodes.end()) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

- (BOOL)hasBookmarksOrFolders {
  return self.sharedState.tableViewDisplayedRootNode &&
         !self.sharedState.tableViewDisplayedRootNode->empty();
}

- (BOOL)allowsNewFolder {
  // When the current root node has been removed remotely (becomes NULL),
  // creating new folder is forbidden.
  return self.sharedState.tableViewDisplayedRootNode != NULL;
}

- (CGFloat)contentPosition {
  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.bookmarkModel->root_node()) {
    return 0;
  }
  // Divided the scroll position by cell height so that it will stay correct in
  // case the cell height is changed in future.
  return self.sharedState.tableView.contentOffset.y /
         [BookmarkHomeSharedState cellHeightPt];
}

- (void)setContentPosition:(CGFloat)position {
  // The scroll position was divided by the cell height when stored.
  [self.sharedState.tableView
      setContentOffset:CGPointMake(
                           0,
                           position * [BookmarkHomeSharedState cellHeightPt])];
}

- (void)navigateAway {
  [self.sharedState.editingFolderCell stopEdit];
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Stop edit of current bookmark folder name, if any.
  [self.sharedState.editingFolderCell stopEdit];
}

#pragma mark - BookmarkModelBridgeObserver Callbacks

// BookmarkModelBridgeObserver Callbacks
// Instances of this class automatically observe the bookmark model.
// The bookmark model has loaded.
- (void)bookmarkModelLoaded {
  [self refreshContents];
}

// The node has changed, but not its children.
- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  // The root folder changed. Do nothing.
  if (bookmarkNode == self.sharedState.tableViewDisplayedRootNode) {
    return;
  }

  // A specific cell changed. Reload, if currently shown.
  if ([self itemForNode:bookmarkNode] != nil) {
    [self refreshContents];
  }
}

// The node has not changed, but its children have.
- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // The current root folder's children changed. Reload everything.
  // (When adding new folder, table is already been updated. So no need to
  // reload here.)
  if (bookmarkNode == self.sharedState.tableViewDisplayedRootNode &&
      !self.sharedState.addingNewFolder) {
    [self refreshContents];
    return;
  }
}

// The node has moved to a new parent folder.
- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (oldParent == self.sharedState.tableViewDisplayedRootNode ||
      newParent == self.sharedState.tableViewDisplayedRootNode) {
    // A folder was added or removed from the current root folder.
    [self refreshContents];
  }
}

// |node| was deleted from |folder|.
- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  if (self.sharedState.tableViewDisplayedRootNode == node) {
    self.sharedState.tableViewDisplayedRootNode = NULL;
    [self refreshContents];
  }
}

// All non-permanent nodes have been removed.
- (void)bookmarkModelRemovedAllNodes {
  // TODO(crbug.com/695749) Check if this case is applicable in the new UI.
}

- (void)bookmarkNodeFaviconChanged:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  // Only urls have favicons.
  DCHECK(bookmarkNode->is_url());

  // Update image of corresponding cell.
  BookmarkHomeNodeItem* nodeItem = [self itemForNode:bookmarkNode];
  if (!nodeItem) {
    return;
  }

  // Check that this cell is visible.
  NSIndexPath* indexPath =
      [self.sharedState.tableViewModel indexPathForItem:nodeItem];
  NSArray* visiblePaths = [self.sharedState.tableView indexPathsForVisibleRows];
  if (![visiblePaths containsObject:indexPath]) {
    return;
  }

  // Get the favicon from cache directly. (no need to fetch from server)
  [self loadFaviconAtIndexPath:indexPath continueToGoogleServer:NO];
}

// Row selection of the tableView will be cleared after reloadData.  This
// function is used to restore the row selection.  It also updates editNodes in
// case some selected nodes are removed.
- (void)restoreRowSelection {
  // Create a new editNodes set to check if some selected nodes are removed.
  std::set<const bookmarks::BookmarkNode*> newEditNodes;

  // Add selected nodes to editNodes only if they are not removed (still exist
  // in the table).
  NSArray<TableViewItem*>* items = [self.sharedState.tableViewModel
      itemsInSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  for (TableViewItem* item in items) {
    BookmarkHomeNodeItem* nodeItem =
        base::mac::ObjCCastStrict<BookmarkHomeNodeItem>(item);
    const BookmarkNode* node = nodeItem.bookmarkNode;
    if (self.sharedState.editNodes.find(node) !=
        self.sharedState.editNodes.end()) {
      newEditNodes.insert(node);
      // Reselect the row of this node.
      NSIndexPath* itemPath =
          [self.sharedState.tableViewModel indexPathForItem:nodeItem];
      [self.sharedState.tableView
          selectRowAtIndexPath:itemPath
                      animated:NO
                scrollPosition:UITableViewScrollPositionNone];
    }
  }

  // if editNodes is changed, update it and tell BookmarkTableViewDelegate.
  if (self.sharedState.editNodes.size() != newEditNodes.size()) {
    self.sharedState.editNodes = newEditNodes;
    [self.delegate bookmarkTableView:self
                   selectedEditNodes:self.sharedState.editNodes];
  }
}

- (BOOL)shouldShowPromoCell {
  return self.sharedState.promoVisible;
}

- (void)refreshContents {
  [self computeBookmarkTableViewData];
  [self showEmptyOrLoadingSpinnerBackgroundIfNeeded];
  [self cancelAllFaviconLoads];
  [self.delegate bookmarkTableViewRefreshContextBar:self];
  [self.sharedState.editingFolderCell stopEdit];
  [self.sharedState.tableView reloadData];
  if (self.sharedState.currentlyInEditMode &&
      !self.sharedState.editNodes.empty()) {
    [self restoreRowSelection];
  }
}

// Returns the bookmark node associated with |indexPath|.
- (const BookmarkNode*)nodeAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];

  if (item.type == BookmarkHomeItemTypeBookmark) {
    BookmarkHomeNodeItem* nodeItem =
        base::mac::ObjCCastStrict<BookmarkHomeNodeItem>(item);
    return nodeItem.bookmarkNode;
  }

  NOTREACHED();
  return nullptr;
}

// Computes the bookmarks table view based on the current root node.
- (void)computeBookmarkTableViewData {
  // Regenerate the list of all bookmarks.
  if ([self.sharedState.tableViewModel
          hasSectionForSectionIdentifier:
              BookmarkHomeSectionIdentifierBookmarks]) {
    [self.sharedState.tableViewModel
        removeSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  }
  [self.sharedState.tableViewModel
      addSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];

  if (!self.sharedState.bookmarkModel->loaded() ||
      !self.sharedState.tableViewDisplayedRootNode) {
    return;
  }

  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.bookmarkModel->root_node()) {
    [self generateTableViewDataForRootNode];
    return;
  }
  [self generateTableViewData];
}

// Generate the table view data when the current root node is a child node.
- (void)generateTableViewData {
  if (!self.sharedState.tableViewDisplayedRootNode) {
    return;
  }
  // Add all bookmarks and folders of the current root node to the table.
  int childCount = self.sharedState.tableViewDisplayedRootNode->child_count();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* node =
        self.sharedState.tableViewDisplayedRootNode->GetChild(i);
    BookmarkHomeNodeItem* nodeItem =
        [[BookmarkHomeNodeItem alloc] initWithType:BookmarkHomeItemTypeBookmark
                                      bookmarkNode:node];
    [self.sharedState.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  }
}

// Generate the table view data when the current root node is the outermost
// root.
- (void)generateTableViewDataForRootNode {
  // Add "Mobile Bookmarks" to the table.
  const BookmarkNode* mobileNode =
      self.sharedState.bookmarkModel->mobile_node();
  BookmarkHomeNodeItem* mobileItem =
      [[BookmarkHomeNodeItem alloc] initWithType:BookmarkHomeItemTypeBookmark
                                    bookmarkNode:mobileNode];
  [self.sharedState.tableViewModel
                      addItem:mobileItem
      toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];

  // Add "Bookmarks Bar" and "Other Bookmarks" only when they are not empty.
  const BookmarkNode* bookmarkBar =
      self.sharedState.bookmarkModel->bookmark_bar_node();
  if (!bookmarkBar->empty()) {
    BookmarkHomeNodeItem* barItem =
        [[BookmarkHomeNodeItem alloc] initWithType:BookmarkHomeItemTypeBookmark
                                      bookmarkNode:bookmarkBar];
    [self.sharedState.tableViewModel
                        addItem:barItem
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  }

  const BookmarkNode* otherBookmarks =
      self.sharedState.bookmarkModel->other_node();
  if (!otherBookmarks->empty()) {
    BookmarkHomeNodeItem* otherItem =
        [[BookmarkHomeNodeItem alloc] initWithType:BookmarkHomeItemTypeBookmark
                                      bookmarkNode:otherBookmarks];
    [self.sharedState.tableViewModel
                        addItem:otherItem
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  }
}

// If the current root node is the outermost root, check if we need to show the
// spinner backgound.  Otherwise, check if we need to show the empty background.
- (void)showEmptyOrLoadingSpinnerBackgroundIfNeeded {
  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.bookmarkModel->root_node()) {
    if (self.sharedState.bookmarkModel->HasNoUserCreatedBookmarksOrFolders() &&
        _syncedSessionsObserver->IsSyncing()) {
      [self showLoadingSpinnerBackground];
    } else {
      [self hideLoadingSpinnerBackground];
    }
    return;
  }

  if (![self hasBookmarksOrFolders]) {
    [self showEmptyBackground];
  } else {
    // Hides the empty bookmarks background if it is showing.
    self.sharedState.tableView.backgroundView = nil;
  }
}

// Shows loading spinner background view.
- (void)showLoadingSpinnerBackground {
  if (!self.spinnerView) {
    self.spinnerView = [[BookmarkHomeWaitingView alloc]
          initWithFrame:self.sharedState.tableView.bounds
        backgroundColor:[UIColor clearColor]];
    [self.spinnerView startWaiting];
  }
  self.sharedState.tableView.backgroundView = self.spinnerView;
}

// Hide the loading spinner if it is showing.
- (void)hideLoadingSpinnerBackground {
  if (self.spinnerView) {
    [self.spinnerView stopWaitingWithCompletion:^{
      [UIView animateWithDuration:0.2
          animations:^{
            self.spinnerView.alpha = 0.0;
          }
          completion:^(BOOL finished) {
            self.sharedState.tableView.backgroundView = nil;
            self.spinnerView = nil;
          }];
    }];
  }
}

// Shows empty bookmarks background view.
- (void)showEmptyBackground {
  if (!self.emptyTableBackgroundView) {
    // Set up the background view shown when the table is empty.
    self.emptyTableBackgroundView = [[BookmarkEmptyBackground alloc]
        initWithFrame:self.sharedState.tableView.bounds];
    self.emptyTableBackgroundView.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    self.emptyTableBackgroundView.text =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL);
    self.emptyTableBackgroundView.frame = self.sharedState.tableView.bounds;
  }
  self.sharedState.tableView.backgroundView = self.emptyTableBackgroundView;
}

- (BookmarkHomeNodeItem*)itemForNode:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  NSArray<TableViewItem*>* items = [self.sharedState.tableViewModel
      itemsInSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  for (TableViewItem* item in items) {
    if (item.type == BookmarkHomeItemTypeBookmark) {
      BookmarkHomeNodeItem* nodeItem =
          base::mac::ObjCCastStrict<BookmarkHomeNodeItem>(item);
      if (nodeItem.bookmarkNode == bookmarkNode) {
        return nodeItem;
      }
    }
  }
  return nil;
}

- (void)updateCellAtIndexPath:(NSIndexPath*)indexPath
                    withImage:(UIImage*)image
              backgroundColor:(UIColor*)backgroundColor
                    textColor:(UIColor*)textColor
                 fallbackText:(NSString*)fallbackText {
  BookmarkTableCell* cell =
      [self.sharedState.tableView cellForRowAtIndexPath:indexPath];
  if (!cell) {
    return;
  }

  if (image) {
    [cell setImage:image];
  } else {
    [cell setPlaceholderText:fallbackText
                   textColor:textColor
             backgroundColor:backgroundColor];
  }
}

- (void)updateCellAtIndexPath:(NSIndexPath*)indexPath
          withLargeIconResult:(const favicon_base::LargeIconResult&)result
                 fallbackText:(NSString*)fallbackText {
  UIImage* favIcon = nil;
  UIColor* backgroundColor = nil;
  UIColor* textColor = nil;

  if (result.bitmap.is_valid()) {
    scoped_refptr<base::RefCountedMemory> data = result.bitmap.bitmap_data;
    favIcon = [UIImage
        imageWithData:[NSData dataWithBytes:data->front() length:data->size()]];
    fallbackText = nil;
    // Update the time when the icon was last requested - postpone thus the
    // automatic eviction of the favicon from the favicon database.
    IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState)
        ->TouchIconFromGoogleServer(result.bitmap.icon_url);
  } else if (result.fallback_icon_style) {
    backgroundColor =
        skia::UIColorFromSkColor(result.fallback_icon_style->background_color);
    textColor =
        skia::UIColorFromSkColor(result.fallback_icon_style->text_color);
  }

  [self updateCellAtIndexPath:indexPath
                    withImage:favIcon
              backgroundColor:backgroundColor
                    textColor:textColor
                 fallbackText:fallbackText];
}

// Cancels all async loads of favicons. Subclasses should call this method when
// the bookmark model is going through significant changes, then manually call
// loadFaviconAtIndexPath: for everything that needs to be loaded; or
// just reload relevant cells.
- (void)cancelAllFaviconLoads {
  _faviconTaskTracker.TryCancelAll();
}

- (void)cancelLoadingFaviconAtIndexPath:(NSIndexPath*)indexPath {
  _faviconTaskTracker.TryCancel(
      _faviconLoadTasks[IntegerPair(indexPath.section, indexPath.item)]);
}

// Asynchronously loads favicon for given index path. The loads are cancelled
// upon cell reuse automatically.  When the favicon is not found in cache, try
// loading it from a Google server if |continueToGoogleServer| is YES,
// otherwise, use the fall back icon style.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
        continueToGoogleServer:(BOOL)continueToGoogleServer {
  const bookmarks::BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  if (node->is_folder()) {
    return;
  }

  CGFloat scale = [UIScreen mainScreen].scale;
  CGFloat desiredFaviconSizeInPixel =
      scale * [BookmarkHomeSharedState desiredFaviconSizePt];
  CGFloat minFaviconSizeInPixel =
      scale * [BookmarkHomeSharedState minFaviconSizePt];

  // Start loading a favicon.
  __weak BookmarkTableView* weakSelf = self;
  GURL blockURL(node->url());
  NSString* fallbackText =
      base::SysUTF16ToNSString(favicon::GetFallbackIconText(blockURL));
  void (^faviconLoadedFromCacheBlock)(const favicon_base::LargeIconResult&) = ^(
      const favicon_base::LargeIconResult& result) {
    BookmarkTableView* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    // TODO(crbug.com/697329) When fetching icon from server to replace existing
    // cache is allowed, fetch icon from server here when cached icon is smaller
    // than the desired size.
    if (!result.bitmap.is_valid() && continueToGoogleServer &&
        strongSelf.sharedState.faviconDownloadCount <
            [BookmarkHomeSharedState maxDownloadFaviconCount]) {
      void (^faviconLoadedFromServerBlock)(
          favicon_base::GoogleFaviconServerRequestStatus status) =
          ^(const favicon_base::GoogleFaviconServerRequestStatus status) {
            if (status ==
                favicon_base::GoogleFaviconServerRequestStatus::SUCCESS) {
              BookmarkTableView* strongSelf = weakSelf;
              // GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache
              // is not cancellable.  So need to check if node has been changed
              // before proceeding to favicon update.
              if (!strongSelf ||
                  [strongSelf nodeAtIndexPath:indexPath] != node) {
                return;
              }
              // Favicon should be ready in cache now.  Fetch it again.
              [strongSelf loadFaviconAtIndexPath:indexPath
                          continueToGoogleServer:NO];
            }
          };  // faviconLoadedFromServerBlock

      strongSelf.sharedState.faviconDownloadCount++;
      IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState)
          ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
              favicon::FaviconServerFetcherParams::CreateForMobile(
                  node->url(), minFaviconSizeInPixel,
                  desiredFaviconSizeInPixel),
              /*may_page_url_be_private=*/true, kTrafficAnnotation,
              base::BindBlockArc(faviconLoadedFromServerBlock));
    }
    [strongSelf updateCellAtIndexPath:indexPath
                  withLargeIconResult:result
                         fallbackText:fallbackText];
  };  // faviconLoadedFromCacheBlock

  base::CancelableTaskTracker::TaskId taskId =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState)
          ->GetLargeIconOrFallbackStyle(
              node->url(), minFaviconSizeInPixel, desiredFaviconSizeInPixel,
              base::BindBlockArc(faviconLoadedFromCacheBlock),
              &_faviconTaskTracker);
  _faviconLoadTasks[IntegerPair(indexPath.section, indexPath.item)] = taskId;
}

- (BOOL)isUrlOrFolder:(const BookmarkNode*)node {
  return node->type() == BookmarkNode::URL ||
         node->type() == BookmarkNode::FOLDER;
}

#pragma mark - SyncedSessionsObserver

- (void)reloadSessions {
  // Nothing to do.
}

- (void)onSyncStateChanged {
  // Permanent nodes ("Bookmarks Bar", "Other Bookmarks") at the root node might
  // be added after syncing.  So we need to refresh here.
  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.bookmarkModel->root_node()) {
    [self refreshContents];
    return;
  }
  [self showEmptyOrLoadingSpinnerBackgroundIfNeeded];
}

@end
