// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_table_view.h"

#include "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/core/fallback_url_util.h"
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
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_waiting_view.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_signin_promo_cell.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/FlexibleHeader/src/MDCFlexibleHeaderView.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Minimal acceptable favicon size, in points.
const CGFloat kMinFaviconSizePt = 16.0;

// Desired favicon size, in points.
const CGFloat kDesiredFaviconSizePt = 32.0;

// Cell height, in points.
const CGFloat kCellHeightPt = 56.0;

// Minimium spacing between keyboard and the titleText when creating new folder,
// in points.
const CGFloat kKeyboardSpacing = 16.0;

// Max number of favicon download requests in the lifespan of this tableView.
const NSUInteger kMaxDownloadFaviconCount = 50;

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
                                BookmarkTableCellTitleEditDelegate,
                                SyncedSessionsObserver,
                                UIGestureRecognizerDelegate,
                                UITableViewDataSource,
                                UITableViewDelegate> {
  // A vector of bookmark nodes to display in the table view.
  std::vector<const BookmarkNode*> _bookmarkItems;

  // The current root node of this table view.
  const BookmarkNode* _currentRootNode;

  // The newly created folder node its name is being edited.
  const BookmarkNode* _editingFolderNode;

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

  // True if the promo is visible.
  BOOL _promoVisible;

  // Set of nodes being editing currently.
  std::set<const bookmarks::BookmarkNode*> _editNodes;
}

// The model holding bookmark data.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// The browser state.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The delegate for actions on the table.
@property(nonatomic, weak) id<BookmarkTableViewDelegate> delegate;
// Background shown when there is no bookmarks or folders at the current root
// node.
@property(nonatomic, strong) BookmarkEmptyBackground* emptyTableBackgroundView;
// The loading spinner background which appears when syncing.
@property(nonatomic, strong) BookmarkHomeWaitingView* spinnerView;

// Presenter for showing signin UI.
@property(nonatomic, readonly, weak) id<SigninPresenter> presenter;

// Section indices.
@property(nonatomic, readonly, assign) NSInteger promoSection;
@property(nonatomic, readonly, assign) NSInteger bookmarksSection;
@property(nonatomic, readonly, assign) NSInteger sectionCount;

// If a new folder is being added currently.
@property(nonatomic, assign) BOOL addingNewFolder;

// The cell for the newly created folder while its name is being edited. Set
// to nil once the editing completes. Corresponds to |_editingFolderNode|.
@property(nonatomic, weak) BookmarkTableCell* editingFolderCell;

// Counts the number of favicon download requests from Google server in the
// lifespan of this tableView.
@property(nonatomic, assign) NSUInteger faviconDownloadCount;

@end

@implementation BookmarkTableView

@synthesize tableView = _tableView;
@synthesize headerView = _headerView;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize browserState = _browserState;
@synthesize delegate = _delegate;
@synthesize emptyTableBackgroundView = _emptyTableBackgroundView;
@synthesize spinnerView = _spinnerView;
@synthesize editing = _editing;
@synthesize presenter = _presenter;
@synthesize addingNewFolder = _addingNewFolder;
@synthesize editingFolderCell = _editingFolderCell;
@synthesize faviconDownloadCount = _faviconDownloadCount;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:(id<BookmarkTableViewDelegate>)delegate
                            rootNode:(const BookmarkNode*)rootNode
                               frame:(CGRect)frame
                           presenter:(id<SigninPresenter>)presenter {
  self = [super initWithFrame:frame];
  if (self) {
    DCHECK(rootNode);
    _browserState = browserState;
    _delegate = delegate;
    _currentRootNode = rootNode;
    _presenter = presenter;

    // Set up connection to the BookmarkModel.
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(browserState);

    // Set up observers.
    _modelBridge =
        std::make_unique<bookmarks::BookmarkModelBridge>(self, _bookmarkModel);
    _syncedSessionsObserver =
        std::make_unique<synced_sessions::SyncedSessionsObserverBridge>(
            self, _browserState);

    [self computeBookmarkTableViewData];

    // Set promo state before the tableview is created.
    [self promoStateChangedAnimated:NO];

    // Create and setup tableview.
    _tableView =
        [[UITableView alloc] initWithFrame:frame style:UITableViewStylePlain];
    self.tableView.accessibilityIdentifier = @"bookmarksTableView";
    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    if (@available(iOS 11.0, *)) {
      self.tableView.contentInsetAdjustmentBehavior =
          UIScrollViewContentInsetAdjustmentNever;
    }
    self.tableView.estimatedRowHeight = kCellHeightPt;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    // Remove extra rows.
    self.tableView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.tableView.allowsMultipleSelectionDuringEditing = YES;
    UILongPressGestureRecognizer* longPressRecognizer =
        [[UILongPressGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(handleLongPress:)];
    longPressRecognizer.numberOfTouchesRequired = 1;
    longPressRecognizer.delegate = self;
    [self.tableView addGestureRecognizer:longPressRecognizer];
    [self addSubview:self.tableView];
    [self bringSubviewToFront:self.tableView];

    [self registerForKeyboardNotifications];
    [self showEmptyOrLoadingSpinnerBackgroundIfNeeded];
  }
  return self;
}

- (void)dealloc {
  [self removeKeyboardObservers];
  _tableView.dataSource = nil;
  _tableView.delegate = nil;
  _faviconTaskTracker.TryCancelAll();
}

#pragma mark - Public

- (void)promoStateChangedAnimated:(BOOL)animated {
  // We show promo cell only on the root view, that is when showing
  // the permanent nodes.
  BOOL promoVisible =
      ((_currentRootNode == self.bookmarkModel->root_node()) &&
       [self.delegate bookmarkTableViewShouldShowPromoCell:self]);

  if (promoVisible == _promoVisible) {
    return;
  }

  _promoVisible = promoVisible;
  if (_promoVisible) {
    [self.delegate.signinPromoViewMediator signinPromoViewVisible];
  } else if (![self.delegate
                     .signinPromoViewMediator isInvalidClosedOrNeverVisible]) {
    // When the sign-in view is closed, the promo state changes, but
    // -[SigninPromoViewMediator signinPromoViewHidden] should not be called.
    [self.delegate.signinPromoViewMediator signinPromoViewHidden];
  }
  [self.tableView reloadData];
}

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  NSIndexPath* indexPath =
      [NSIndexPath indexPathForRow:0 inSection:self.promoSection];
  BookmarkTableSigninPromoCell* signinPromoCell =
      static_cast<BookmarkTableSigninPromoCell*>(
          [self.tableView cellForRowAtIndexPath:indexPath]);
  if (!signinPromoCell) {
    return;
  }
  // Should always reconfigure the cell size even if it has to be reloaded,
  // to make sure it has the right size to compute the cell size.
  [configurator configureSigninPromoView:signinPromoCell.signinPromoView];
  if (identityChanged) {
    // The section should be reload to update the cell height.
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:self.promoSection];
    [self.tableView reloadSections:indexSet
                  withRowAnimation:UITableViewRowAnimationNone];
  }
}

- (void)addNewFolder {
  [self.editingFolderCell stopEdit];
  if (!_currentRootNode) {
    return;
  }
  self.addingNewFolder = YES;
  base::string16 folderTitle = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_GROUP_DEFAULT_NAME));
  _editingFolderNode = self.bookmarkModel->AddFolder(
      _currentRootNode, _currentRootNode->child_count(), folderTitle);

  _bookmarkItems.push_back(_editingFolderNode);

  // Insert the new folder cell at the end of the table.
  NSIndexPath* newRowIndexPath =
      [NSIndexPath indexPathForRow:_bookmarkItems.size() - 1
                         inSection:self.bookmarksSection];
  NSMutableArray* newRowIndexPaths =
      [[NSMutableArray alloc] initWithObjects:newRowIndexPath, nil];
  [self.tableView beginUpdates];
  [self.tableView insertRowsAtIndexPaths:newRowIndexPaths
                        withRowAnimation:UITableViewRowAnimationNone];
  [self.tableView endUpdates];

  // Scroll to the end of the table
  [self.tableView scrollToRowAtIndexPath:newRowIndexPath
                        atScrollPosition:UITableViewScrollPositionBottom
                                animated:YES];
}

- (void)setEditing:(BOOL)editing {
  // If not in editing mode but the tableView's editing is ON, it means the
  // table is waiting for a swipe-to-delete confirmation.  In this case, we need
  // to close the confirmation by setting tableView.editing to NO.
  if (!_editing && self.tableView.editing) {
    self.tableView.editing = NO;
  }
  [self.editingFolderCell stopEdit];
  _editing = editing;
  [self resetEditNodes];
  [self.tableView setEditing:editing animated:YES];
}

- (const std::set<const bookmarks::BookmarkNode*>&)editNodes {
  return _editNodes;
}

- (std::vector<const bookmarks::BookmarkNode*>)getEditNodesInVector {
  // Create a vector of edit nodes in the same order as the nodes in folder.
  std::vector<const bookmarks::BookmarkNode*> nodes;
  int childCount = _currentRootNode->child_count();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* node = _currentRootNode->GetChild(i);
    if (_editNodes.find(node) != _editNodes.end()) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

- (BOOL)hasBookmarksOrFolders {
  return _currentRootNode && !_currentRootNode->empty();
}

- (BOOL)allowsNewFolder {
  // When the current root node has been removed remotely (becomes NULL),
  // creating new folder is forbidden.
  return _currentRootNode != NULL;
}

- (CGFloat)contentPosition {
  if (_currentRootNode == self.bookmarkModel->root_node()) {
    return 0;
  }
  // Divided the scroll position by cell height so that it will stay correct in
  // case the cell height is changed in future.
  return self.tableView.contentOffset.y / kCellHeightPt;
}

- (void)setContentPosition:(CGFloat)position {
  // The scroll position was divided by the cell height when stored.
  [self.tableView setContentOffset:CGPointMake(0, position * kCellHeightPt)];
}

- (void)navigateAway {
  [self.editingFolderCell stopEdit];
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Stop edit of current bookmark folder name, if any.
  [self.editingFolderCell stopEdit];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return self.sectionCount;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  if (section == self.bookmarksSection) {
    return _bookmarkItems.size();
  }
  if (section == self.promoSection) {
    return 1;
  }

  NOTREACHED();
  return -1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.promoSection) {
    BookmarkTableSigninPromoCell* signinPromoCell = [self.tableView
        dequeueReusableCellWithIdentifier:[BookmarkTableSigninPromoCell
                                              reuseIdentifier]];
    if (signinPromoCell == nil) {
      signinPromoCell =
          [[BookmarkTableSigninPromoCell alloc] initWithFrame:CGRectZero];
    }
    signinPromoCell.signinPromoView.delegate =
        self.delegate.signinPromoViewMediator;
    [[self.delegate.signinPromoViewMediator createConfigurator]
        configureSigninPromoView:signinPromoCell.signinPromoView];
    signinPromoCell.selectionStyle = UITableViewCellSelectionStyleNone;
    [self.delegate.signinPromoViewMediator signinPromoViewVisible];
    return signinPromoCell;
  }

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  BookmarkTableCell* cell = [tableView
      dequeueReusableCellWithIdentifier:[BookmarkTableCell reuseIdentifier]];

  if (cell == nil) {
    cell = [[BookmarkTableCell alloc]
        initWithReuseIdentifier:[BookmarkTableCell reuseIdentifier]];
  }
  [cell setNode:node];

  if (node == _editingFolderNode) {
    // Delay starting edit, so that the cell is fully created.
    dispatch_async(dispatch_get_main_queue(), ^{
      self.editingFolderCell = cell;
      [cell startEdit];
      cell.textDelegate = self;
    });
  }

  // Cancel previous load attempts.
  [self cancelLoadingFaviconAtIndexPath:indexPath];
  // Load the favicon from cache.  If not found, try fetching it from a Google
  // Server.
  [self loadFaviconAtIndexPath:indexPath continueToGoogleServer:YES];

  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.promoSection) {
    // Ignore promo section edit.
    return NO;
  }

  // We enable the swipe-to-delete gesture and reordering control for nodes of
  // type URL or Folder, and not the permanent ones.
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  return [self isUrlOrFolder:node];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.promoSection) {
    // Ignore promo section editing style.
    return;
  }

  if (editingStyle == UITableViewCellEditingStyleDelete) {
    const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
    std::set<const BookmarkNode*> nodes;
    nodes.insert(node);
    [self.delegate bookmarkTableView:self selectedNodesForDeletion:nodes];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    canMoveRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.promoSection) {
    // Ignore promo section.
    return NO;
  }

  return YES;
}

- (void)tableView:(UITableView*)tableView
    moveRowAtIndexPath:(NSIndexPath*)sourceIndexPath
           toIndexPath:(NSIndexPath*)destinationIndexPath {
  if (sourceIndexPath.row == destinationIndexPath.row) {
    return;
  }
  const BookmarkNode* node = [self nodeAtIndexPath:sourceIndexPath];
  // Calculations: Assume we have 3 nodes A B C. Node positions are A(0), B(1),
  // C(2) respectively. When we move A to after C, we are moving node at index 0
  // to 3 (position after C is 3, in terms of the existing contents). Hence add
  // 1 when moving forward. When moving backward, if C(2) is moved to Before B,
  // we move node at index 2 to index 1 (position before B is 1, in terms of the
  // existing contents), hence no change in index is necessary. It is required
  // to make these adjustments because this is how bookmark_model handles move
  // operations.
  int newPosition = sourceIndexPath.row < destinationIndexPath.row
                        ? destinationIndexPath.row + 1
                        : destinationIndexPath.row;
  [self.delegate bookmarkTableView:self
                       didMoveNode:node
                        toPosition:newPosition];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.bookmarksSection) {
    return kCellHeightPt;
  }
  return UITableViewAutomaticDimension;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.bookmarksSection) {
    const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
    DCHECK(node);
    // If table is in edit mode, record all the nodes added to edit set.
    if (self.editing) {
      _editNodes.insert(node);
      [self.delegate bookmarkTableView:self selectedEditNodes:_editNodes];
      return;
    }
    [self.editingFolderCell stopEdit];
    if (node->is_folder()) {
      [self.delegate bookmarkTableView:self selectedFolderForNavigation:node];
    } else {
      // Open URL. Pass this to the delegate.
      [self.delegate bookmarkTableView:self
              selectedUrlForNavigation:node->url()];
    }
  }
  // Deselect row.
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.bookmarksSection && self.editing) {
    const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
    DCHECK(node);
    _editNodes.erase(node);
    [self.delegate bookmarkTableView:self selectedEditNodes:_editNodes];
  }
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
  if (bookmarkNode == _currentRootNode) {
    return;
  }

  // A specific cell changed. Reload, if currently shown.
  if (std::find(_bookmarkItems.begin(), _bookmarkItems.end(), bookmarkNode) !=
      _bookmarkItems.end()) {
    [self refreshContents];
  }
}

// The node has not changed, but its children have.
- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // The current root folder's children changed. Reload everything.
  // (When adding new folder, table is already been updated. So no need to
  // reload here.)
  if (bookmarkNode == _currentRootNode && !self.addingNewFolder) {
    [self refreshContents];
    return;
  }
}

// The node has moved to a new parent folder.
- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (oldParent == _currentRootNode || newParent == _currentRootNode) {
    // A folder was added or removed from the current root folder.
    [self refreshContents];
  }
}

// |node| was deleted from |folder|.
- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  if (_currentRootNode == node) {
    _currentRootNode = NULL;
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
  NSIndexPath* indexPath = [self indexPathForNode:bookmarkNode];

  if (!indexPath) {
    return;
  }

  // Check that this cell is visible.
  NSArray* visiblePaths = [self.tableView indexPathsForVisibleRows];
  if (![visiblePaths containsObject:indexPath]) {
    return;
  }

  // Get the favicon from cache directly. (no need to fetch from server)
  [self loadFaviconAtIndexPath:indexPath continueToGoogleServer:NO];
}

#pragma mark - Sections

- (NSInteger)promoSection {
  return [self shouldShowPromoCell] ? 0 : -1;
}

- (NSInteger)bookmarksSection {
  return [self shouldShowPromoCell] ? 1 : 0;
}

- (NSInteger)sectionCount {
  return [self shouldShowPromoCell] ? 2 : 1;
}

#pragma mark - Gesture recognizer

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Ignore long press in edit mode.
  if (self.editing) {
    return NO;
  }
  return YES;
}

#pragma mark - Private

- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer {
  if (self.editing ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }
  CGPoint touchPoint = [gestureRecognizer locationInView:self.tableView];
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:touchPoint];
  if (indexPath == nil || indexPath.section != self.bookmarksSection) {
    return;
  }

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  // Disable the long press gesture if it is a permanent node (not an URL or
  // Folder).
  if (!node || ![self isUrlOrFolder:node]) {
    return;
  }

  [self.delegate bookmarkTableView:self showContextMenuForNode:node];
}

- (void)resetEditNodes {
  _editNodes.clear();
  if (self.editing) {
    // Also update viewcontroler that the edit nodes changed, if in edit mode.
    [self.delegate bookmarkTableView:self selectedEditNodes:_editNodes];
  }
}

// Row selection of the tableView will be cleared after reloadData.  This
// function is used to restore the row selection.  It also updates editNodes in
// case some selected nodes are removed.
- (void)restoreRowSelection {
  // Create a new editNodes set to check if some selected nodes are removed.
  std::set<const bookmarks::BookmarkNode*> newEditNodes;

  // Add selected nodes to editNodes only if they are not removed (still exist
  // in the table).
  for (size_t index = 0; index < _bookmarkItems.size(); ++index) {
    const BookmarkNode* node = _bookmarkItems[index];
    if (_editNodes.find(node) != _editNodes.end()) {
      newEditNodes.insert(node);
      // Reselect the row of this node.
      [self.tableView
          selectRowAtIndexPath:[NSIndexPath
                                   indexPathForRow:index
                                         inSection:self.bookmarksSection]
                      animated:NO
                scrollPosition:UITableViewScrollPositionNone];
    }
  }

  // if editNodes is changed, update it and tell BookmarkTableViewDelegate.
  if (_editNodes.size() != newEditNodes.size()) {
    _editNodes = newEditNodes;
    [self.delegate bookmarkTableView:self selectedEditNodes:_editNodes];
  }
}

- (BOOL)shouldShowPromoCell {
  return _promoVisible;
}

- (void)refreshContents {
  [self computeBookmarkTableViewData];
  [self showEmptyOrLoadingSpinnerBackgroundIfNeeded];
  [self cancelAllFaviconLoads];
  [self.delegate bookmarkTableViewRefreshContextBar:self];
  [self.editingFolderCell stopEdit];
  [self.tableView reloadData];
  if (self.editing && !_editNodes.empty()) {
    [self restoreRowSelection];
  }
}

// Returns the bookmark node associated with |indexPath|.
- (const BookmarkNode*)nodeAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.bookmarksSection) {
    return _bookmarkItems[indexPath.row];
  }

  NOTREACHED();
  return nullptr;
}

// Computes the bookmarks table view based on the current root node.
- (void)computeBookmarkTableViewData {
  // Regenerate the list of all bookmarks.
  _bookmarkItems.clear();

  if (!self.bookmarkModel->loaded() || !_currentRootNode) {
    return;
  }

  if (_currentRootNode == self.bookmarkModel->root_node()) {
    [self generateTableViewDataForRootNode];
    return;
  }
  [self generateTableViewData];
}

// Generate the table view data when the current root node is a child node.
- (void)generateTableViewData {
  if (!_currentRootNode) {
    return;
  }
  // Add all bookmarks and folders of the current root node to the table.
  int childCount = _currentRootNode->child_count();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* node = _currentRootNode->GetChild(i);
    _bookmarkItems.push_back(node);
  }
}

// Generate the table view data when the current root node is the outermost
// root.
- (void)generateTableViewDataForRootNode {
  // Add "Mobile Bookmarks" to the table.
  _bookmarkItems.push_back(self.bookmarkModel->mobile_node());

  // Add "Bookmarks Bar" and "Other Bookmarks" only when they are not empty.
  const BookmarkNode* bookmarkBar = self.bookmarkModel->bookmark_bar_node();
  if (!bookmarkBar->empty()) {
    _bookmarkItems.push_back(bookmarkBar);
  }

  const BookmarkNode* otherBookmarks = self.bookmarkModel->other_node();
  if (!otherBookmarks->empty()) {
    _bookmarkItems.push_back(otherBookmarks);
  }
}

// If the current root node is the outermost root, check if we need to show the
// spinner backgound.  Otherwise, check if we need to show the empty background.
- (void)showEmptyOrLoadingSpinnerBackgroundIfNeeded {
  if (_currentRootNode == self.bookmarkModel->root_node()) {
    if (self.bookmarkModel->HasNoUserCreatedBookmarksOrFolders() &&
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
    self.tableView.backgroundView = nil;
  }
}

// Shows loading spinner background view.
- (void)showLoadingSpinnerBackground {
  if (!self.spinnerView) {
    self.spinnerView =
        [[BookmarkHomeWaitingView alloc] initWithFrame:self.tableView.bounds
                                       backgroundColor:[UIColor clearColor]];
    [self.spinnerView startWaiting];
  }
  self.tableView.backgroundView = self.spinnerView;
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
            self.tableView.backgroundView = nil;
            self.spinnerView = nil;
          }];
    }];
  }
}

// Shows empty bookmarks background view.
- (void)showEmptyBackground {
  if (!self.emptyTableBackgroundView) {
    // Set up the background view shown when the table is empty.
    self.emptyTableBackgroundView =
        [[BookmarkEmptyBackground alloc] initWithFrame:self.tableView.bounds];
    self.emptyTableBackgroundView.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    self.emptyTableBackgroundView.text =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL);
    self.emptyTableBackgroundView.frame = self.tableView.bounds;
  }
  self.tableView.backgroundView = self.emptyTableBackgroundView;
}

- (NSIndexPath*)indexPathForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  NSIndexPath* indexPath = nil;
  std::vector<const BookmarkNode*>::iterator it =
      std::find(_bookmarkItems.begin(), _bookmarkItems.end(), bookmarkNode);
  if (it != _bookmarkItems.end()) {
    ptrdiff_t index = std::distance(_bookmarkItems.begin(), it);
    indexPath =
        [NSIndexPath indexPathForRow:index inSection:self.bookmarksSection];
  }
  return indexPath;
}

- (void)updateCellAtIndexPath:(NSIndexPath*)indexPath
                    withImage:(UIImage*)image
              backgroundColor:(UIColor*)backgroundColor
                    textColor:(UIColor*)textColor
                 fallbackText:(NSString*)fallbackText {
  BookmarkTableCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
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
  CGFloat desiredFaviconSizeInPixel = scale * kDesiredFaviconSizePt;
  CGFloat minFaviconSizeInPixel = scale * kMinFaviconSizePt;

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
        strongSelf.faviconDownloadCount < kMaxDownloadFaviconCount) {
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

      strongSelf.faviconDownloadCount++;
      IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState)
          ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
              node->url(), minFaviconSizeInPixel, desiredFaviconSizeInPixel,
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

#pragma mark - Keyboard

- (void)registerForKeyboardNotifications {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWasShown:)
             name:UIKeyboardDidShowNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillBeHidden:)
             name:UIKeyboardWillHideNotification
           object:nil];
}

- (void)removeKeyboardObservers {
  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter removeObserver:self
                                name:UIKeyboardDidShowNotification
                              object:nil];
  [notificationCenter removeObserver:self
                                name:UIKeyboardWillHideNotification
                              object:nil];
}

// Called when the UIKeyboardDidShowNotification is sent
- (void)keyboardWasShown:(NSNotification*)aNotification {
  if (![self.delegate isAtTopOfNavigation:self]) {
    return;
  }
  NSDictionary* info = [aNotification userInfo];
  CGFloat keyboardTop =
      [[info objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue].origin.y;
  CGFloat tableBottom =
      CGRectGetMaxY([self convertRect:self.tableView.frame toView:nil]);
  CGFloat shiftY = tableBottom - keyboardTop + kKeyboardSpacing;

  if (shiftY >= 0) {
    UIEdgeInsets previousContentInsets = self.tableView.contentInset;
    // Shift the content inset to prevent the editing content from being hidden
    // by the keyboard.
    UIEdgeInsets contentInsets =
        UIEdgeInsetsMake(previousContentInsets.top, 0.0, shiftY, 0.0);
    self.tableView.contentInset = contentInsets;
    self.tableView.scrollIndicatorInsets = contentInsets;
  }
}

// Called when the UIKeyboardWillHideNotification is sent
- (void)keyboardWillBeHidden:(NSNotification*)aNotification {
  if (![self.delegate isAtTopOfNavigation:self]) {
    return;
  }
  UIEdgeInsets previousContentInsets = self.tableView.contentInset;
  // Restore the content inset now that the keyboard has been hidden.
  UIEdgeInsets contentInsets =
      UIEdgeInsetsMake(previousContentInsets.top, 0, 0, 0);
  self.tableView.contentInset = contentInsets;
  self.tableView.scrollIndicatorInsets = contentInsets;
}

#pragma mark - SyncedSessionsObserver

- (void)reloadSessions {
  // Nothing to do.
}

- (void)onSyncStateChanged {
  // Permanent nodes ("Bookmarks Bar", "Other Bookmarks") at the root node might
  // be added after syncing.  So we need to refresh here.
  if (_currentRootNode == self.bookmarkModel->root_node()) {
    [self refreshContents];
    return;
  }
  [self showEmptyOrLoadingSpinnerBackgroundIfNeeded];
}

#pragma mark - BookmarkTableCellTextFieldDelegate

- (void)textDidChangeTo:(NSString*)newName {
  DCHECK(_editingFolderNode);
  self.addingNewFolder = NO;
  if (newName.length > 0) {
    self.bookmarkModel->SetTitle(_editingFolderNode,
                                 base::SysNSStringToUTF16(newName));
  }
  _editingFolderNode = NULL;
  self.editingFolderCell = nil;
  [self refreshContents];
}

#pragma mark UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (scrollView == self.headerView.trackingScrollView) {
    [self.headerView trackingScrollViewDidScroll];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  if (scrollView == self.headerView.trackingScrollView) {
    [self.headerView trackingScrollViewDidEndDecelerating];
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  if (scrollView == self.headerView.trackingScrollView) {
    [self.headerView trackingScrollViewDidEndDraggingWillDecelerate:decelerate];
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  if (scrollView == self.headerView.trackingScrollView) {
    [self.headerView
        trackingScrollViewWillEndDraggingWithVelocity:velocity
                                  targetContentOffset:targetContentOffset];
  }
}

@end
