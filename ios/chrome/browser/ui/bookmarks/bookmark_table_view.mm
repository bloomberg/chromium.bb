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
#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_collection_view_background.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_promo_cell.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_signin_promo_cell.h"
#import "ios/chrome/browser/ui/sync/synced_sessions_bridge.h"
#include "ios/chrome/grit/ios_strings.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Minimal acceptable favicon size, in points.
CGFloat kMinFaviconSizePt = 16;

// Cell height, in points.
CGFloat kCellHeightPt = 56.0;
}

using bookmarks::BookmarkNode;

// Used to store a pair of NSIntegers when storing a NSIndexPath in C++
// collections.
using IntegerPair = std::pair<NSInteger, NSInteger>;

@interface BookmarkTableView ()<BookmarkTablePromoCellDelegate,
                                SigninPromoViewConsumer,
                                UITableViewDataSource,
                                UITableViewDelegate,
                                BookmarkModelBridgeObserver> {
  // A vector of bookmark nodes to display in the table view.
  std::vector<const BookmarkNode*> _bookmarkItems;
  const BookmarkNode* _currentRootNode;

  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
  // Map of favicon load tasks for each index path. Used to keep track of
  // pending favicon load operations so that they can be cancelled upon cell
  // reuse. Keys are (section, item) pairs of cell index paths.
  std::map<IntegerPair, base::CancelableTaskTracker::TaskId> _faviconLoadTasks;
  // Task tracker used for async favicon loads.
  base::CancelableTaskTracker _faviconTaskTracker;

  // Mediator, helper for the sign-in promo view.
  SigninPromoViewMediator* _signinPromoViewMediator;

  // True if the promo is visible.
  BOOL _promoVisible;
}

// The UITableView to show bookmarks.
@property(nonatomic, strong) UITableView* tableView;
// The model holding bookmark data.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// The browser state.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The delegate for actions on the table.
@property(nonatomic, assign) id<BookmarkTableViewDelegate> delegate;
// Background view of the collection view shown when there is no items.
@property(nonatomic, strong)
    BookmarkCollectionViewBackground* emptyTableBackgroundView;

// Section indices.
@property(nonatomic, readonly, assign) NSInteger promoSection;
@property(nonatomic, readonly, assign) NSInteger bookmarksSection;
@property(nonatomic, readonly, assign) NSInteger sectionCount;

@end

@implementation BookmarkTableView

@synthesize bookmarkModel = _bookmarkModel;
@synthesize browserState = _browserState;
@synthesize tableView = _tableView;
@synthesize delegate = _delegate;
@synthesize emptyTableBackgroundView = _emptyTableBackgroundView;

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterIntegerPref(prefs::kIosBookmarkSigninPromoDisplayedCount,
                                0);
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:(id<BookmarkTableViewDelegate>)delegate
                            rootNode:(const BookmarkNode*)rootNode
                               frame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    DCHECK(rootNode);
    _browserState = browserState;
    _delegate = delegate;
    _currentRootNode = rootNode;

    // Set up connection to the BookmarkModel.
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(browserState);

    // Set up observers.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));

    [self computeBookmarkTableViewData];

    // Set promo state before the tableview is created.
    [self promoStateChangedAnimated:NO];

    // Create and setup tableview.
    self.tableView =
        [[UITableView alloc] initWithFrame:frame style:UITableViewStylePlain];
    self.tableView.dataSource = self;
    self.tableView.delegate = self;

    // Use iOS8's self sizing feature to compute row height. However,
    // this reduces the row height of bookmarks section from 56 to 45
    // TODO(crbug.com/695749): Fix the bookmark section row height to 56.
    self.tableView.estimatedRowHeight = kCellHeightPt;
    self.tableView.rowHeight = UITableViewAutomaticDimension;

    // Remove extra rows.
    self.tableView.tableFooterView = [[UIView alloc] initWithFrame:CGRectZero];
    self.tableView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self addSubview:self.tableView];
    [self bringSubviewToFront:self.tableView];

    // Set up the background view shown when the table is empty.
    self.emptyTableBackgroundView =
        [[BookmarkCollectionViewBackground alloc] initWithFrame:frame];
    self.emptyTableBackgroundView.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    self.emptyTableBackgroundView.text =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL);
    [self updateEmptyBackground];
    [self addSubview:self.emptyTableBackgroundView];
  }
  return self;
}

- (void)dealloc {
  [_signinPromoViewMediator signinPromoViewRemoved];
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
       [self.delegate bookmarkTableViewShouldShowPromoCell:self]) ||
      (_signinPromoViewMediator &&
       _signinPromoViewMediator.signinPromoViewState ==
           ios::SigninPromoViewState::SigninStarted);

  if (promoVisible == _promoVisible)
    return;

  _promoVisible = promoVisible;

  if (experimental_flags::IsSigninPromoEnabled()) {
    if (!promoVisible) {
      _signinPromoViewMediator.consumer = nil;
      [_signinPromoViewMediator signinPromoViewRemoved];
      _signinPromoViewMediator = nil;
    } else {
      _signinPromoViewMediator = [[SigninPromoViewMediator alloc]
          initWithBrowserState:_browserState
                   accessPoint:signin_metrics::AccessPoint::
                                   ACCESS_POINT_BOOKMARK_MANAGER];
      _signinPromoViewMediator.consumer = self;
      [_signinPromoViewMediator signinPromoViewVisible];
    }
  }
  [self.tableView reloadData];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return self.sectionCount;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  if (section == self.bookmarksSection)
    return _bookmarkItems.size();
  if (section == self.promoSection)
    return 1;

  NOTREACHED();
  return -1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  // TODO(crbug.com/695749): Introduce a custom separator for bookmarks
  // section, so that we don't show a separator after promo section.
  if (indexPath.section == self.promoSection) {
    if (experimental_flags::IsSigninPromoEnabled()) {
      BookmarkTableSigninPromoCell* signinPromoCell = [self.tableView
          dequeueReusableCellWithIdentifier:[BookmarkTableSigninPromoCell
                                                reuseIdentifier]];
      if (signinPromoCell == nil) {
        signinPromoCell =
            [[BookmarkTableSigninPromoCell alloc] initWithFrame:CGRectZero];
      }
      signinPromoCell.signinPromoView.delegate = _signinPromoViewMediator;
      [[_signinPromoViewMediator createConfigurator]
          configureSigninPromoView:signinPromoCell.signinPromoView];
      __weak BookmarkTableView* weakSelf = self;
      signinPromoCell.closeButtonAction = ^() {
        [weakSelf signinPromoCloseButtonAction];
      };
      return signinPromoCell;
    } else {
      BookmarkTablePromoCell* promoCell = [self.tableView
          dequeueReusableCellWithIdentifier:[BookmarkTablePromoCell
                                                reuseIdentifier]];
      if (promoCell == nil) {
        promoCell = [[BookmarkTablePromoCell alloc] initWithFrame:CGRectZero];
      }
      promoCell.delegate = self;
      return promoCell;
    }
  }

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  BookmarkTableCell* cell = [tableView
      dequeueReusableCellWithIdentifier:[BookmarkTableCell reuseIdentifier]];

  if (cell == nil) {
    cell = [[BookmarkTableCell alloc]
        initWithReuseIdentifier:[BookmarkTableCell reuseIdentifier]];
  }
  [cell setNode:node];

  [self loadFaviconAtIndexPath:indexPath];
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
  return node->type() == BookmarkNode::URL ||
         node->type() == BookmarkNode::FOLDER;
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

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.bookmarksSection) {
    const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
    DCHECK(node);
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

#pragma mark - BookmarkTablePromoCellDelegate

- (void)bookmarkTablePromoCellDidTapSignIn:
    (BookmarkTablePromoCell*)bookmarkTablePromoCell {
  [self.delegate bookmarkTableViewShowSignIn:self];
}

- (void)bookmarkTablePromoCellDidTapDismiss:
    (BookmarkTablePromoCell*)bookmarkTablePromoCell {
  [self.delegate bookmarkTableViewDismissPromo:self];
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  DCHECK(_signinPromoViewMediator);
  NSIndexPath* indexPath =
      [NSIndexPath indexPathForRow:0 inSection:self.promoSection];
  BookmarkTableSigninPromoCell* signinPromoCell =
      static_cast<BookmarkTableSigninPromoCell*>(
          [self.tableView cellForRowAtIndexPath:indexPath]);
  if (!signinPromoCell)
    return;
  // Should always reconfigure the cell size even if it has to be reloaded.
  [configurator configureSigninPromoView:signinPromoCell.signinPromoView];
  if (identityChanged) {
    // The section should be reload to update the cell height.
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:self.promoSection];
    [self.tableView reloadSections:indexSet
                  withRowAnimation:UITableViewRowAnimationNone];
  }
}

- (void)signinDidFinish {
  [self promoStateChangedAnimated:NO];
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
  if (bookmarkNode == _currentRootNode)
    return;

  // A specific cell changed. Reload, if currently shown.
  if (std::find(_bookmarkItems.begin(), _bookmarkItems.end(), bookmarkNode) !=
      _bookmarkItems.end()) {
    [self refreshContents];
  }
}

// The node has not changed, but its children have.
- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // The current root folder's children changed. Reload everything.
  if (bookmarkNode == _currentRootNode) {
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

  if (!indexPath)
    return;

  // Check that this cell is visible.
  NSArray* visiblePaths = [self.tableView indexPathsForVisibleRows];
  if (![visiblePaths containsObject:indexPath])
    return;

  [self loadFaviconAtIndexPath:indexPath];
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

#pragma mark - Private

// Removes the sign-in promo view.
- (void)signinPromoCloseButtonAction {
  [_signinPromoViewMediator signinPromoViewClosed];
  [_delegate bookmarkTableViewDismissPromo:self];
}

- (BOOL)shouldShowPromoCell {
  return _promoVisible;
}

- (void)refreshContents {
  [self computeBookmarkTableViewData];
  [self updateEmptyBackground];
  [self cancelAllFaviconLoads];
  [self.tableView reloadData];
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
  if (!self.bookmarkModel->loaded() || _currentRootNode == NULL)
    return;

  // Regenerate the list of all bookmarks.
  _bookmarkItems.clear();
  int childCount = _currentRootNode->child_count();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* node = _currentRootNode->GetChild(i);
    _bookmarkItems.push_back(node);
  }
}

- (void)updateEmptyBackground {
  self.emptyTableBackgroundView.alpha =
      _currentRootNode != NULL && _currentRootNode->child_count() > 0 ? 0 : 1;
}

- (NSIndexPath*)indexPathForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  NSIndexPath* indexPath = nil;
  std::vector<const BookmarkNode*>::iterator it =
      std::find(_bookmarkItems.begin(), _bookmarkItems.end(), bookmarkNode);
  if (it != _bookmarkItems.end()) {
    ptrdiff_t index = std::distance(_bookmarkItems.begin(), it);
    indexPath = [NSIndexPath indexPathForRow:index inSection:1];
  }
  return indexPath;
}

- (void)updateCellAtIndexPath:(NSIndexPath*)indexPath
                    withImage:(UIImage*)image
              backgroundColor:(UIColor*)backgroundColor
                    textColor:(UIColor*)textColor
                 fallbackText:(NSString*)text {
  BookmarkTableCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  if (!cell)
    return;

  if (image) {
    [cell setImage:image];
  } else {
    [cell setPlaceholderText:text
                   textColor:textColor
             backgroundColor:backgroundColor];
  }
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
// upon cell reuse automatically.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath {
  const bookmarks::BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  if (node->is_folder()) {
    return;
  }

  // Cancel previous load attempts.
  [self cancelLoadingFaviconAtIndexPath:indexPath];

  // Start loading a favicon.
  __weak BookmarkTableView* weakSelf = self;
  GURL blockURL(node->url());
  void (^faviconBlock)(const favicon_base::LargeIconResult&) =
      ^(const favicon_base::LargeIconResult& result) {
        BookmarkTableView* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        UIImage* favIcon = nil;
        UIColor* backgroundColor = nil;
        UIColor* textColor = nil;
        NSString* fallbackText = nil;
        if (result.bitmap.is_valid()) {
          scoped_refptr<base::RefCountedMemory> data =
              result.bitmap.bitmap_data;
          favIcon = [UIImage imageWithData:[NSData dataWithBytes:data->front()
                                                          length:data->size()]];
        } else if (result.fallback_icon_style) {
          backgroundColor = skia::UIColorFromSkColor(
              result.fallback_icon_style->background_color);
          textColor =
              skia::UIColorFromSkColor(result.fallback_icon_style->text_color);

          fallbackText =
              base::SysUTF16ToNSString(favicon::GetFallbackIconText(blockURL));
        }

        [strongSelf updateCellAtIndexPath:indexPath
                                withImage:favIcon
                          backgroundColor:backgroundColor
                                textColor:textColor
                             fallbackText:fallbackText];
      };

  CGFloat scale = [UIScreen mainScreen].scale;
  CGFloat preferredSize = scale * [BookmarkTableCell preferredImageSize];
  CGFloat minSize = scale * kMinFaviconSizePt;

  base::CancelableTaskTracker::TaskId taskId =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState)
          ->GetLargeIconOrFallbackStyle(node->url(), minSize, preferredSize,
                                        base::BindBlockArc(faviconBlock),
                                        &_faviconTaskTracker);
  _faviconLoadTasks[IntegerPair(indexPath.section, indexPath.item)] = taskId;
}

@end
