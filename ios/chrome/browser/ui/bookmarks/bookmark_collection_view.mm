// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view.h"

#import <UIKit/UIGestureRecognizerSubclass.h>
#include <algorithm>
#include <map>
#include <memory>

#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
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
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_cells.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view_background.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_cell.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_signin_promo_cell.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

namespace {
// Computes the cell size based on width.
CGSize PreferredCellSizeForWidth(UICollectionViewCell* cell, CGFloat width) {
  CGRect cellFrame = cell.frame;
  cellFrame.size.width = width;
  cellFrame.size.height = CGFLOAT_MAX;
  cell.frame = cellFrame;
  [cell setNeedsLayout];
  [cell layoutIfNeeded];
  CGSize result =
      [cell systemLayoutSizeFittingSize:UILayoutFittingCompressedSize
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityDefaultLow];
  cellFrame.size = result;
  cell.frame = cellFrame;
  return result;
}

// Used to store a pair of NSIntegers when storing a NSIndexPath in C++
// collections.
using IntegerPair = std::pair<NSInteger, NSInteger>;

// The margin between the side of the view and the first and last tile.
CGFloat rowMarginTablet = 24.0;
CGFloat rowHeight = 48.0;
// Minimal acceptable favicon size, in points.
CGFloat minFaviconSizePt = 16;

// Delay in seconds to which the empty background view will be shown when the
// collection view is empty.
// This delay should not be too small to let enough time to load bookmarks
// from network.
const NSTimeInterval kShowEmptyBookmarksBackgroundRefreshDelay = 1.0;
}

@interface BookmarkCollectionView ()<BookmarkPromoCellDelegate,
                                     SigninPromoViewConsumer,
                                     UICollectionViewDataSource,
                                     UICollectionViewDelegateFlowLayout,
                                     UIGestureRecognizerDelegate> {
  // A vector of folders to display in the collection view.
  std::vector<const BookmarkNode*> _subFolders;
  // A vector of bookmark urls to display in the collection view.
  std::vector<const BookmarkNode*> _subItems;

  // True if the promo is visible.
  BOOL _promoVisible;

  // Mediator, helper for the sign-in promo view.
  SigninPromoViewMediator* _signinPromoViewMediator;

  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
  ios::ChromeBrowserState* _browserState;

  // Map of favicon load tasks for each index path. Used to keep track of
  // pending favicon load operations so that they can be cancelled upon cell
  // reuse. Keys are (section, item) pairs of cell index paths.
  std::map<IntegerPair, base::CancelableTaskTracker::TaskId> _faviconLoadTasks;
  // Task tracker used for async favicon loads.
  base::CancelableTaskTracker _faviconTaskTracker;
}

// Redefined to be readwrite.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// Redefined to be readwrite.
@property(nonatomic, strong) UICollectionView* collectionView;
// Redefined to be readwrite.
@property(nonatomic, assign) BOOL editing;
// Detects a long press on a cell.
@property(nonatomic, strong) UILongPressGestureRecognizer* longPressRecognizer;
// Background view of the collection view shown when there is no items.
@property(nonatomic, strong)
    BookmarkCollectionViewBackground* emptyCollectionBackgroundView;
// Shadow to display over the content.
@property(nonatomic, strong) UIView* shadow;

@property(nonatomic, assign) const bookmarks::BookmarkNode* folder;

// Section indices.
@property(nonatomic, readonly, assign) NSInteger promoSection;
@property(nonatomic, readonly, assign) NSInteger folderSection;
@property(nonatomic, readonly, assign) NSInteger itemsSection;
@property(nonatomic, readonly, assign) NSInteger sectionCount;

@end

@implementation BookmarkCollectionView
@synthesize delegate = _delegate;
@synthesize folder = _folder;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize collectionView = _collectionView;
@synthesize editing = _editing;
@synthesize emptyCollectionBackgroundView = _emptyCollectionBackgroundView;
@synthesize loader = _loader;
@synthesize longPressRecognizer = _longPressRecognizer;
@synthesize browserState = _browserState;
@synthesize shadow = _shadow;

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterIntegerPref(prefs::kIosBookmarkSigninPromoDisplayedCount,
                                0);
}

#pragma mark - Initialization

- (void)setupViews {
  self.backgroundColor = bookmark_utils_ios::mainBackgroundColor();
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];

  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:self.bounds
                         collectionViewLayout:layout];
  self.collectionView = collectionView;
  self.collectionView.backgroundColor = [UIColor clearColor];
  self.collectionView.autoresizingMask =
      UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
  self.collectionView.alwaysBounceVertical = YES;
  self.collectionView.delegate = self;
  self.collectionView.dataSource = self;
  [self.collectionView registerClass:[BookmarkFolderCell class]
          forCellWithReuseIdentifier:[BookmarkFolderCell reuseIdentifier]];
  [self.collectionView registerClass:[BookmarkItemCell class]
          forCellWithReuseIdentifier:[BookmarkItemCell reuseIdentifier]];
  [self.collectionView registerClass:[BookmarkHeaderView class]
          forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
                 withReuseIdentifier:[BookmarkHeaderView reuseIdentifier]];
  [self.collectionView
                   registerClass:[BookmarkHeaderSeparatorView class]
      forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
             withReuseIdentifier:[BookmarkHeaderSeparatorView reuseIdentifier]];
  [self.collectionView registerClass:[BookmarkPromoCell class]
          forCellWithReuseIdentifier:[BookmarkPromoCell reuseIdentifier]];
  [self.collectionView registerClass:[BookmarkSigninPromoCell class]
          forCellWithReuseIdentifier:[BookmarkSigninPromoCell reuseIdentifier]];

  [self addSubview:self.collectionView];

  // Set up the background view shown when the collection is empty.
  BookmarkCollectionViewBackground* emptyCollectionBackgroundView =
      [[BookmarkCollectionViewBackground alloc] initWithFrame:CGRectZero];
  self.emptyCollectionBackgroundView = emptyCollectionBackgroundView;
  self.emptyCollectionBackgroundView.autoresizingMask =
      UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
  self.emptyCollectionBackgroundView.alpha = 0;
  self.emptyCollectionBackgroundView.text = [self textWhenCollectionIsEmpty];

  self.emptyCollectionBackgroundView.frame = self.collectionView.bounds;
  self.collectionView.backgroundView = self.emptyCollectionBackgroundView;

  [self updateShadow];

  self.longPressRecognizer = [[UILongPressGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(longPress:)];
  self.longPressRecognizer.delegate = self;
  [self.collectionView addGestureRecognizer:self.longPressRecognizer];
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                               frame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _browserState = browserState;

    // Set up connection to the BookmarkModel.
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(browserState);

    // Set up observers.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));

    [self setupViews];
    [self updateCollectionView];
  }
  return self;
}

- (void)dealloc {
  [_signinPromoViewMediator signinPromoViewRemoved];
  _collectionView.dataSource = nil;
  _collectionView.delegate = nil;
  UIView* moi = _collectionView;
  dispatch_async(dispatch_get_main_queue(), ^{
    // A collection view with a layout that uses a dynamic animator (aka
    // something that changes the layout over time) will crash if it is
    // deallocated while the animation is currently playing.
    // Apparently if a tick has been dispatched it will execute, invoking a
    // method on the deallocated collection.
    // The only purpose of this block is to retain the collection view for a
    // while, giving the layout a chance to perform its last tick.
    [moi self];
  });
  _faviconTaskTracker.TryCancelAll();
}

#pragma mark - BookmarkHomePrimaryView

- (void)changeOrientation:(UIInterfaceOrientation)orientation {
  [self updateCollectionView];
}

- (CGFloat)contentPositionInPortraitOrientation {
  if (IsPortrait())
    return self.collectionView.contentOffset.y;

  // In short landscape mode and portrait mode, there are 2 cells per row.
  if ([self wideLandscapeMode])
    return self.collectionView.contentOffset.y;

  // In wide landscape mode, there are 3 cells per row.
  return self.collectionView.contentOffset.y * 3 / 2.0;
}

- (void)applyContentPosition:(CGFloat)position {
  if (IsLandscape() && [self wideLandscapeMode]) {
    position = position * 2 / 3.0;
  }

  CGFloat y =
      MIN(position,
          [self.collectionView.collectionViewLayout collectionViewContentSize]
              .height);
  self.collectionView.contentOffset =
      CGPointMake(self.collectionView.contentOffset.x, y);
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  if (self.editing == editing)
    return;

  _editing = editing;
  [UIView animateWithDuration:animated ? 0.2 : 0.0
                        delay:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     self.shadow.alpha = editing ? 0.0 : 1.0;
                   }
                   completion:nil];

  // If the promo is active this means that it is removed and added as the edit
  // mode changes, making reloading the data mandatory.
  if ([self isPromoActive]) {
    [self.collectionView reloadData];
    [self.collectionView.collectionViewLayout invalidateLayout];
  } else {
    // Update the visual state of the bookmark cells without reloading the
    // section.
    // This prevents flickering of images that need to be asynchronously
    // reloaded.
    NSArray* indexPaths = [self.collectionView indexPathsForVisibleItems];
    for (NSIndexPath* indexPath in indexPaths) {
      [self updateEditingStateOfCellAtIndexPath:indexPath
                          animateMenuVisibility:animated
                           animateSelectedState:NO];
    }
  }
  [self promoStateChangedAnimated:animated];
}

- (void)setScrollsToTop:(BOOL)scrollsToTop {
  self.collectionView.scrollsToTop = scrollsToTop;
}

#pragma mark - Public

- (void)resetFolder:(const BookmarkNode*)folder {
  DCHECK(folder->is_folder());
  self.folder = folder;
  [self updateCollectionView];
}

- (void)setDelegate:(id<BookmarkCollectionViewDelegate>)delegate {
  _delegate = delegate;
  [self promoStateChangedAnimated:NO];
}

- (void)collectionViewScrolled {
  [self.delegate bookmarkCollectionViewDidScroll:self];
}

- (void)promoStateChangedAnimated:(BOOL)animated {
  BOOL shouldShowPromo =
      (!self.editing && self.folder &&
       self.folder->type() == BookmarkNode::MOBILE &&
       [self.delegate bookmarkCollectionViewShouldShowPromoCell:self]) ||
      (_signinPromoViewMediator &&
       _signinPromoViewMediator.signinPromoViewState ==
           ios::SigninPromoViewState::SigninStarted);
  if (shouldShowPromo == _promoVisible)
    return;
  // This is awful, but until the old code to do the refresh when switching
  // in and out of edit mode is fixed, this is probably the cleanest thing to
  // do.
  _promoVisible = shouldShowPromo;
  if (experimental_flags::IsSigninPromoEnabled()) {
    if (!_promoVisible) {
      _signinPromoViewMediator.consumer = nil;
      [_signinPromoViewMediator signinPromoViewRemoved];
      _signinPromoViewMediator = nil;
    } else {
      _signinPromoViewMediator =
          [[SigninPromoViewMediator alloc] initWithBrowserState:_browserState];
      _signinPromoViewMediator.consumer = self;
      _signinPromoViewMediator.displayedCountPreferenceKey =
          prefs::kIosBookmarkSigninPromoDisplayedCount;
      _signinPromoViewMediator.alreadySeenSigninViewPreferenceKey =
          prefs::kIosBookmarkPromoAlreadySeen;
      _signinPromoViewMediator.histograms =
          ios::SigninPromoViewHistograms::Bookmarks;
      _signinPromoViewMediator.accessPoint =
          signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER;
      [_signinPromoViewMediator signinPromoViewVisible];
    }
  }
  [self.collectionView reloadData];
}

- (void)wasShown {
  [_signinPromoViewMediator signinPromoViewVisible];
}

- (void)wasHidden {
  [_signinPromoViewMediator signinPromoViewHidden];
}

#pragma mark - Sections

- (NSInteger)promoSection {
  return [self shouldShowPromoCell] ? 0 : -1;
}

- (NSInteger)folderSection {
  return [self shouldShowPromoCell] ? 1 : 0;
}

- (NSInteger)itemsSection {
  return [self shouldShowPromoCell] ? 2 : 1;
}

- (NSInteger)sectionCount {
  return [self shouldShowPromoCell] ? 3 : 2;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [self updateShadow];
}

- (void)updateShadow {
  // Remove the current one, if any.
  [self.shadow removeFromSuperview];

  if (IsCompact(self)) {
    self.shadow =
        bookmark_utils_ios::dropShadowWithWidth(CGRectGetWidth(self.bounds));
  } else {
    self.shadow = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0, CGRectGetWidth(self.bounds),
                                 1 / [[UIScreen mainScreen] scale])];
    self.shadow.backgroundColor = [UIColor colorWithWhite:0.0 alpha:.12];
  }

  [self updateShadowFrame];
  self.shadow.autoresizingMask = UIViewAutoresizingFlexibleWidth;
  if (self.editing)
    self.shadow.alpha = 0.0;

  // Add the new shadow.
  [self addSubview:self.shadow];
}

- (void)updateShadowFrame {
  CGFloat shadowHeight = CGRectGetHeight(self.shadow.frame);
  CGFloat y = std::min<CGFloat>(
      0.0, self.collectionView.contentOffset.y - shadowHeight);
  self.shadow.frame =
      CGRectMake(0, y, CGRectGetWidth(self.bounds), shadowHeight);
}

#pragma mark - BookmarkItemCell callbacks

// Callback received when the user taps the menu button on the cell.
- (void)didTapMenuButton:(BookmarkItemCell*)cell view:(UIView*)view {
  [self didTapMenuButtonAtIndexPath:[self.collectionView indexPathForCell:cell]
                             onView:view
                            forCell:cell];
}

#pragma mark - BookmarkModelBridgeObserver Callbacks
// BookmarkModelBridgeObserver Callbacks
// Instances of this class automatically observe the bookmark model.
// The bookmark model has loaded.
- (void)bookmarkModelLoaded {
  [self updateCollectionView];
}

// The node has changed, but not its children.
- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  // The base folder changed. Do nothing.
  if (bookmarkNode == self.folder)
    return;

  // A specific cell changed. Reload that cell.
  NSIndexPath* indexPath = [self indexPathForNode:bookmarkNode];

  if (indexPath) {
    // TODO(crbug.com/603661): Ideally, we would only reload the relevant index
    // path. However, calling reloadItemsAtIndexPaths:(0,0) immediately after
    // reloadData results in a exception: NSInternalInconsistencyException
    // 'request for index path for global index 2147483645 ...'
    // One solution would be to keep track of whether we've just called
    // reloadData, but that requires experimentation to determine how long we
    // have to wait before we can safely call reloadItemsAtIndexPaths.
    [self updateCollectionView];
  }
}

// The node has not changed, but its children have.
- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // The base folder's children changed. Reload everything.
  if (bookmarkNode == self.folder) {
    [self updateCollectionView];
    return;
  }

  // A subfolder's children changed. Reload that cell.
  if (base::ContainsValue(_subFolders, bookmarkNode)) {
    // TODO(crbug.com/603661): Ideally, we would only reload the relevant index
    // path. However, calling reloadItemsAtIndexPaths:(0,0) immediately after
    // reloadData results in a exception: NSInternalInconsistencyException
    // 'request for index path for global index 2147483645 ...'
    // One solution would be to keep track of whether we've just called
    // reloadData, but that requires experimentation to determine how long we
    // have to wait before we can safely call reloadItemsAtIndexPaths.
    [self updateCollectionView];
  }
}

// The node has moved to a new parent folder.
- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (oldParent == self.folder || newParent == self.folder) {
    // A folder was added or removed from the base folder.
    [self updateCollectionView];
  }
}

// |node| was deleted from |folder|.
- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  if (self.folder == node) {
    self.folder = nil;
    [self updateCollectionView];
  }
}

// All non-permanent nodes have been removed.
- (void)bookmarkModelRemovedAllNodes {
  self.folder = nil;
  [self updateCollectionView];
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
  NSArray* visiblePaths = [self.collectionView indexPathsForVisibleItems];
  if (![visiblePaths containsObject:indexPath])
    return;

  [self loadFaviconAtIndexPath:indexPath];
}

#pragma mark - non-UI

// Called when a user is attempting to select a cell.
// Returning NO prevents the cell from being selected.
- (BOOL)shouldSelectCellAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

// Called when a cell is tapped outside of editing mode.
- (void)didTapCellAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.promoSection) {
    // User tapped inside promo cell but not on one of the buttons. Ignore it.
    return;
  }

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  DCHECK(node);

  if (indexPath.section == self.folderSection) {
    [self.delegate bookmarkCollectionView:self
              selectedFolderForNavigation:node];
  } else {
    RecordBookmarkLaunch(BOOKMARK_LAUNCH_LOCATION_FOLDER);
    [self.delegate bookmarkCollectionView:self
                 selectedUrlForNavigation:node->url()];
  }
}

// Called when a user selected a cell in the editing state.
- (void)didAddCellForEditingAtIndexPath:(NSIndexPath*)indexPath {
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  UICollectionViewCell* cell =
      [self.collectionView cellForItemAtIndexPath:indexPath];
  [self.delegate bookmarkCollectionView:self cell:cell addNodeForEditing:node];
}

- (void)didRemoveCellForEditingAtIndexPath:(NSIndexPath*)indexPath {
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  UICollectionViewCell* cell =
      [self.collectionView cellForItemAtIndexPath:indexPath];
  [self.delegate bookmarkCollectionView:self
                                   cell:cell
                   removeNodeForEditing:node];
}

// Called when a user taps the menu button on a cell.
- (void)didTapMenuButtonAtIndexPath:(NSIndexPath*)indexPath
                             onView:(UIView*)view
                            forCell:(BookmarkItemCell*)cell {
  [self.delegate bookmarkCollectionView:self
                   wantsMenuForBookmark:[self nodeAtIndexPath:indexPath]
                                 onView:view
                                forCell:cell];
}

// Whether a cell should show a button and of which type.
- (bookmark_cell::ButtonType)buttonTypeForCellAtIndexPath:
    (NSIndexPath*)indexPath {
  return self.editing ? bookmark_cell::ButtonNone : bookmark_cell::ButtonMenu;
}

// Whether a long press at the cell at |indexPath| should be allowed.
- (BOOL)allowLongPressForCellAtIndexPath:(NSIndexPath*)indexPath {
  return !self.editing;
}

// The |cell| at |indexPath| received a long press.
- (void)didLongPressCell:(UICollectionViewCell*)cell
             atIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.promoSection) {
    // User long-pressed inside promo cell. Ignore it.
    return;
  }

  [self.delegate bookmarkCollectionView:self
                       didLongPressCell:cell
                            forBookmark:[self nodeAtIndexPath:indexPath]];
}

// Whether the cell has been selected in editing mode.
- (BOOL)cellIsSelectedForEditingAtIndexPath:(NSIndexPath*)indexPath {
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  const std::set<const BookmarkNode*>& editingNodes =
      [self.delegate nodesBeingEdited];
  return editingNodes.find(node) != editingNodes.end();
}

// Updates the collection view based on the current state of all models and
// contextual parameters, such as the interface orientation.
- (void)updateCollectionView {
  if (!self.bookmarkModel->loaded())
    return;

  // Regenerate the list of all bookmarks.
  _subFolders = std::vector<const BookmarkNode*>();
  _subItems = std::vector<const BookmarkNode*>();

  if (self.folder) {
    int childCount = self.folder->child_count();
    for (int i = 0; i < childCount; ++i) {
      const BookmarkNode* node = self.folder->GetChild(i);
      if (node->is_folder())
        _subFolders.push_back(node);
      else
        _subItems.push_back(node);
    }

    bookmark_utils_ios::SortFolders(&_subFolders);
  }

  [self cancelAllFaviconLoads];
  [self.collectionView reloadData];
}

// Returns the bookmark node associated with |indexPath|.
- (const BookmarkNode*)nodeAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.folderSection)
    return _subFolders[indexPath.row];
  if (indexPath.section == self.itemsSection)
    return _subItems[indexPath.row];

  NOTREACHED();
  return nullptr;
}

#pragma mark -

- (NSIndexPath*)indexPathForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  NSIndexPath* indexPath = nil;
  if (bookmarkNode->is_folder()) {
    std::vector<const BookmarkNode*>::iterator it =
        std::find(_subFolders.begin(), _subFolders.end(), bookmarkNode);
    if (it != _subFolders.end()) {
      ptrdiff_t index = std::distance(_subFolders.begin(), it);
      indexPath =
          [NSIndexPath indexPathForRow:index inSection:self.folderSection];
    }
  } else if (bookmarkNode->is_url()) {
    std::vector<const BookmarkNode*>::iterator it =
        std::find(_subItems.begin(), _subItems.end(), bookmarkNode);
    if (it != _subItems.end()) {
      ptrdiff_t index = std::distance(_subItems.begin(), it);
      indexPath =
          [NSIndexPath indexPathForRow:index inSection:self.itemsSection];
    }
  }
  return indexPath;
}

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(UICollectionViewCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.itemsSection) {
    [self loadFaviconAtIndexPath:indexPath];
  }
}

- (CGFloat)interitemSpacingForSectionAtIndex:(NSInteger)section {
  CGFloat interitemSpacing = 0;
  SEL minimumInteritemSpacingSelector = @selector
      (collectionView:layout:minimumInteritemSpacingForSectionAtIndex:);
  if ([self.collectionView.delegate
          respondsToSelector:minimumInteritemSpacingSelector]) {
    id collectionViewDelegate = static_cast<id>(self.collectionView.delegate);
    interitemSpacing =
        [collectionViewDelegate collectionView:self.collectionView
                                              layout:self.collectionView
                                                         .collectionViewLayout
            minimumInteritemSpacingForSectionAtIndex:section];
  } else if ([self.collectionView.collectionViewLayout
                 isKindOfClass:[UICollectionViewFlowLayout class]]) {
    UICollectionViewFlowLayout* flowLayout =
        static_cast<UICollectionViewFlowLayout*>(
            self.collectionView.collectionViewLayout);
    interitemSpacing = flowLayout.minimumInteritemSpacing;
  }
  return interitemSpacing;
}

// The inset of the section.
- (UIEdgeInsets)insetForSectionAtIndex:(NSInteger)section {
  UIEdgeInsets insets = UIEdgeInsetsZero;
  if ([self isPromoSection:section])
    insets = UIEdgeInsetsZero;

  if (IsIPadIdiom()) {
    insets = UIEdgeInsetsMake(10, rowMarginTablet, 0, rowMarginTablet);
  } else {
    insets = UIEdgeInsetsZero;
  }

  if (section == self.folderSection)
    insets.bottom = [self interitemSpacingForSectionAtIndex:section] / 2.;
  else if (section == self.itemsSection)
    insets.top = [self interitemSpacingForSectionAtIndex:section] / 2.;
  else if (section == self.promoSection)
    (void)0;  // No insets to update.
  else
    NOTREACHED();
  return insets;
}

// The size of the cell at |indexPath|.
- (CGSize)cellSizeForIndexPath:(NSIndexPath*)indexPath {
  if ([self isPromoSection:indexPath.section]) {
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    if (!cell) {
      // -[UICollectionView
      // dequeueReusableCellWithReuseIdentifier:forIndexPath:] cannot be used
      // here since this method is called by -[id<UICollectionViewDelegate>
      // collectionView:layout:sizeForItemAtIndexPath:]. This would generate
      // crash: SIGFPE, EXC_I386_DIV.
      if (experimental_flags::IsSigninPromoEnabled()) {
        DCHECK(_signinPromoViewMediator);
        BookmarkSigninPromoCell* signinPromoCell =
            [[BookmarkSigninPromoCell alloc]
                initWithFrame:CGRectMake(0, 0, 1000, 1000)];
        [[_signinPromoViewMediator createConfigurator]
            configureSigninPromoView:signinPromoCell.signinPromoView];
        cell = signinPromoCell;
      } else {
        cell = [[BookmarkPromoCell alloc] init];
      }
    }
    return PreferredCellSizeForWidth(cell, CGRectGetWidth(self.bounds));
  }
  DCHECK(![self isPromoSection:indexPath.section]);
  UIEdgeInsets insets = [self insetForSectionAtIndex:indexPath.section];
  return CGSizeMake(self.bounds.size.width - (insets.right + insets.left),
                    rowHeight);
}

- (BOOL)needsSectionHeaderForSection:(NSInteger)section {
  // Only show header when there is at least one element in the previous
  // section.
  if (section == 0)
    return NO;

  if ([self numberOfItemsInSection:(section - 1)] == 0)
    return NO;

  return YES;
}

#pragma mark - UI

// The size of the header for |section|. A return value of CGSizeZero prevents
// a header from showing.
- (CGSize)headerSizeForSection:(NSInteger)section {
  if ([self needsSectionHeaderForSection:section])
    return CGSizeMake(self.bounds.size.width,
                      [BookmarkHeaderSeparatorView preferredHeight]);

  return CGSizeZero;
}

// Create a cell for display at |indexPath|.
- (UICollectionViewCell*)cellAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == self.promoSection) {
    if (experimental_flags::IsSigninPromoEnabled()) {
      BookmarkSigninPromoCell* signinPromoCell = [self.collectionView
          dequeueReusableCellWithReuseIdentifier:[BookmarkSigninPromoCell
                                                     reuseIdentifier]
                                    forIndexPath:indexPath];
      signinPromoCell.signinPromoView.delegate = _signinPromoViewMediator;
      [[_signinPromoViewMediator createConfigurator]
          configureSigninPromoView:signinPromoCell.signinPromoView];
      __weak BookmarkCollectionView* weakSelf = self;
      signinPromoCell.closeButtonAction = ^() {
        [weakSelf signinPromoCloseButtonAction];
      };
      return signinPromoCell;
    } else {
      BookmarkPromoCell* promoCell = [self.collectionView
          dequeueReusableCellWithReuseIdentifier:[BookmarkPromoCell
                                                     reuseIdentifier]
                                    forIndexPath:indexPath];
      promoCell.delegate = self;
      return promoCell;
    }
  }
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];

  if (indexPath.section == self.folderSection)
    return [self cellForFolder:node indexPath:indexPath];

  BookmarkItemCell* cell = [self cellForBookmark:node indexPath:indexPath];
  return cell;
}

// Removes the sign-in promo view.
- (void)signinPromoCloseButtonAction {
  [_signinPromoViewMediator signinPromoViewClosed];
  [_delegate bookmarkCollectionViewDismissPromo:self];
}

// Create a header view for the element at |indexPath|.
- (UICollectionReusableView*)headerAtIndexPath:(NSIndexPath*)indexPath {
  if (![self needsSectionHeaderForSection:indexPath.section])
    return nil;

  BookmarkHeaderSeparatorView* view = [self.collectionView
      dequeueReusableSupplementaryViewOfKind:
          UICollectionElementKindSectionHeader
                         withReuseIdentifier:[BookmarkHeaderSeparatorView
                                                 reuseIdentifier]
                                forIndexPath:indexPath];
  view.backgroundColor = [UIColor colorWithWhite:1 alpha:1];
  return view;
}

- (NSInteger)numberOfItemsInSection:(NSInteger)section {
  if (section == self.folderSection)
    return _subFolders.size();
  if (section == self.itemsSection)
    return _subItems.size();
  if (section == self.promoSection)
    return 1;

  NOTREACHED();
  return -1;
}

- (NSInteger)numberOfSections {
  return self.sectionCount;
}

#pragma mark - BookmarkPromoCellDelegate

- (void)bookmarkPromoCellDidTapSignIn:(BookmarkPromoCell*)bookmarkPromoCell {
  [self.delegate bookmarkCollectionViewShowSignIn:self];
}

- (void)bookmarkPromoCellDidTapDismiss:(BookmarkPromoCell*)bookmarkPromoCell {
  [self.delegate bookmarkCollectionViewDismissPromo:self];
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  DCHECK(_signinPromoViewMediator);
  NSIndexPath* indexPath =
      [NSIndexPath indexPathForRow:0 inSection:self.promoSection];
  BookmarkSigninPromoCell* signinPromoCell =
      static_cast<BookmarkSigninPromoCell*>(
          [self.collectionView cellForItemAtIndexPath:indexPath]);
  if (!signinPromoCell)
    return;
  // Should always reconfigure the cell size even if it has to be reloaded.
  // -[BookmarkCollectionView cellSizeForIndexPath:] uses the current
  // cell to compute its height.
  [configurator configureSigninPromoView:signinPromoCell.signinPromoView];
  if (identityChanged) {
    // The section should be reload to update the cell height.
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:self.promoSection];
    [self.collectionView reloadSections:indexSet];
  }
}

- (void)signinDidFinish {
  [self promoStateChangedAnimated:NO];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateShadowFrame];
  [self collectionViewScrolled];
}

#pragma mark - empty background

// Schedules showing or hiding the empty bookmarks background view if the
// collection view is empty by calling showEmptyBackgroundIfNeeded after
// kShowEmptyBookmarksBackgroundRefreshDelay.
// Multiple call to this method will cancel previous scheduled call to
// showEmptyBackgroundIfNeeded before scheduling a new one.
- (void)scheduleEmptyBackgroundVisibilityUpdate {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector
                                           (updateEmptyBackgroundVisibility)
                                             object:nil];
  [self performSelector:@selector(updateEmptyBackgroundVisibility)
             withObject:nil
             afterDelay:kShowEmptyBookmarksBackgroundRefreshDelay];
}

- (BOOL)isCollectionViewEmpty {
  BOOL collectionViewIsEmpty = YES;
  const NSInteger numberOfSections = [self numberOfSections];
  NSInteger section = [self shouldShowPromoCell] ? 1 : 0;
  for (; collectionViewIsEmpty && section < numberOfSections; ++section) {
    const NSInteger numberOfItemsInSection =
        [self numberOfItemsInSection:section];
    collectionViewIsEmpty = numberOfItemsInSection == 0;
  }
  return collectionViewIsEmpty;
}

// Shows/hides empty bookmarks background view if the collections view is empty.
- (void)updateEmptyBackgroundVisibility {
  const BOOL showEmptyBackground =
      [self isCollectionViewEmpty] && ![self shouldShowPromoCell];
  [self setEmptyBackgroundVisible:showEmptyBackground];
}

// Shows/hides empty bookmarks background view with an animation.
- (void)setEmptyBackgroundVisible:(BOOL)emptyBackgroundVisible {
  [UIView beginAnimations:@"alpha" context:NULL];
  self.emptyCollectionBackgroundView.alpha = emptyBackgroundVisible ? 1 : 0;
  [UIView commitAnimations];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  const NSInteger numberOfItemsInSection =
      [self numberOfItemsInSection:section];
  const BOOL isCollectionViewEmpty = [self isCollectionViewEmpty];
  self.collectionView.scrollEnabled = !isCollectionViewEmpty;
  if (isCollectionViewEmpty) {
    [self scheduleEmptyBackgroundVisibilityUpdate];
  } else {
    // Hide empty bookmarks now.
    [self setEmptyBackgroundVisible:NO];
  }
  return numberOfItemsInSection;
}

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  const NSInteger numberOfSections = [self numberOfSections];
  const BOOL collectionViewIsEmpty = 0 == numberOfSections;
  self.collectionView.scrollEnabled = !collectionViewIsEmpty;
  if (collectionViewIsEmpty) {
    [self scheduleEmptyBackgroundVisibilityUpdate];
  } else {
    // Hide empty bookmarks now.
    [self setEmptyBackgroundVisible:NO];
  }
  return numberOfSections;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  return [self cellAtIndexPath:indexPath];
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  return [self headerAtIndexPath:indexPath];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  return [self shouldSelectCellAtIndexPath:indexPath];
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  if (self.editing)
    [self toggleSelectedForEditingAtIndexPath:indexPath];
  else
    [self didTapCellAtIndexPath:indexPath];
}

- (void)toggleSelectedForEditingAtIndexPath:(NSIndexPath*)indexPath {
  BOOL selected = [self cellIsSelectedForEditingAtIndexPath:indexPath];
  if (selected)
    [self didRemoveCellForEditingAtIndexPath:indexPath];
  else
    [self didAddCellForEditingAtIndexPath:indexPath];

  [self updateEditingStateOfCellAtIndexPath:indexPath
                      animateMenuVisibility:NO
                       animateSelectedState:YES];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didEndDisplayingCell:(UICollectionViewCell*)cell
      forItemAtIndexPath:(NSIndexPath*)indexPath {
  _faviconTaskTracker.TryCancel(
      _faviconLoadTasks[IntegerPair(indexPath.section, indexPath.item)]);
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (UIEdgeInsets)collectionView:(UICollectionView*)collectionView
                        layout:(UICollectionViewLayout*)collectionViewLayout
        insetForSectionAtIndex:(NSInteger)section {
  return [self insetForSectionAtIndex:section];
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
                                      layout:(UICollectionViewLayout*)
                                                 collectionViewLayout
    minimumInteritemSpacingForSectionAtIndex:(NSInteger)section {
  return [self minimumInteritemSpacingForSectionAtIndex:section];
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
                                 layout:(UICollectionViewLayout*)layout
    minimumLineSpacingForSectionAtIndex:(NSInteger)section {
  return [self minimumLineSpacingForSectionAtIndex:section];
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  return [self cellSizeForIndexPath:indexPath];
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  return [self headerSizeForSection:section];
}

#pragma mark - UIGestureRecognizer Callbacks

- (void)longPress:(UILongPressGestureRecognizer*)recognizer {
  if (self.longPressRecognizer.numberOfTouches != 1 || self.editing)
    return;

  if (self.longPressRecognizer.state == UIGestureRecognizerStateRecognized ||
      self.longPressRecognizer.state == UIGestureRecognizerStateBegan) {
    CGPoint point =
        [self.longPressRecognizer locationOfTouch:0 inView:self.collectionView];
    NSIndexPath* indexPath =
        [self.collectionView indexPathForItemAtPoint:point];
    if (!indexPath)
      return;

    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];

    // Notify the subclass that long press has been received.
    if (cell)
      [self didLongPressCell:cell atIndexPath:indexPath];
  }
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  DCHECK(gestureRecognizer == self.longPressRecognizer);
  CGPoint point =
      [gestureRecognizer locationOfTouch:0 inView:self.collectionView];
  NSIndexPath* indexPath = [self.collectionView indexPathForItemAtPoint:point];
  if (!indexPath)
    return NO;
  return [self allowLongPressForCellAtIndexPath:indexPath];
}

#pragma mark - Convenience methods

- (void)updateCellAtIndexPath:(NSIndexPath*)indexPath
                    withImage:(UIImage*)image
              backgroundColor:(UIColor*)backgroundColor
                    textColor:(UIColor*)textColor
                 fallbackText:(NSString*)text {
  BookmarkItemCell* cell = base::mac::ObjCCast<BookmarkItemCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
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

- (BookmarkItemCell*)cellForBookmark:(const BookmarkNode*)node
                           indexPath:(NSIndexPath*)indexPath {
  DCHECK(![self isPromoSection:indexPath.section]);

  BOOL selected = [self cellIsSelectedForEditingAtIndexPath:indexPath];

  BookmarkItemCell* cell = [self.collectionView
      dequeueReusableCellWithReuseIdentifier:[BookmarkItemCell reuseIdentifier]
                                forIndexPath:indexPath];

  [cell updateWithTitle:bookmark_utils_ios::TitleForBookmarkNode(node)];
  [cell setSelectedForEditing:selected animated:NO];
  [cell setButtonTarget:self action:@selector(didTapMenuButton:view:)];

  [self updateEditingStateOfCell:cell
                     atIndexPath:indexPath
           animateMenuVisibility:NO
            animateSelectedState:NO];

  [self loadFaviconAtIndexPath:indexPath];

  return cell;
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
  // Cancel previous load attempts.
  [self cancelLoadingFaviconAtIndexPath:indexPath];

  // Start loading a favicon.
  __weak BookmarkCollectionView* weakSelf = self;
  const bookmarks::BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  GURL blockURL(node->url());
  void (^faviconBlock)(const favicon_base::LargeIconResult&) =
      ^(const favicon_base::LargeIconResult& result) {
        BookmarkCollectionView* strongSelf = weakSelf;
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
  CGFloat preferredSize = scale * [BookmarkItemCell preferredImageSize];
  CGFloat minSize = scale * minFaviconSizePt;

  base::CancelableTaskTracker::TaskId taskId =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState)
          ->GetLargeIconOrFallbackStyle(node->url(), minSize, preferredSize,
                                        base::BindBlockArc(faviconBlock),
                                        &_faviconTaskTracker);
  _faviconLoadTasks[IntegerPair(indexPath.section, indexPath.item)] = taskId;
}

- (BookmarkFolderCell*)cellForFolder:(const BookmarkNode*)node
                           indexPath:(NSIndexPath*)indexPath {
  DCHECK(![self isPromoSection:indexPath.section]);
  BookmarkFolderCell* cell = [self.collectionView
      dequeueReusableCellWithReuseIdentifier:[BookmarkFolderCell
                                                 reuseIdentifier]
                                forIndexPath:indexPath];
  [self updateEditingStateOfCell:cell
                     atIndexPath:indexPath
           animateMenuVisibility:NO
            animateSelectedState:NO];
  [cell updateWithTitle:bookmark_utils_ios::TitleForBookmarkNode(node)];
  [cell setButtonTarget:self action:@selector(didTapMenuButton:view:)];

  return cell;
}

// Updates the editing state for the cell.
- (void)updateEditingStateOfCell:(BookmarkCell*)cell
                     atIndexPath:(NSIndexPath*)indexPath
           animateMenuVisibility:(BOOL)animateMenuVisibility
            animateSelectedState:(BOOL)animateSelectedState {
  BOOL selected = [self cellIsSelectedForEditingAtIndexPath:indexPath];
  [cell setSelectedForEditing:selected animated:animateSelectedState];
  BookmarkItemCell* itemCell = static_cast<BookmarkItemCell*>(cell);
  [itemCell showButtonOfType:[self buttonTypeForCellAtIndexPath:indexPath]
                    animated:animateMenuVisibility];
}

// |animateMenuVisibility| refers to whether the change in the visibility of the
// menu button is animated.
// |animateSelectedState| refers to whether the change in the selected state (in
// editing mode) of the cell is animated.
// This method updates the visibility of the menu button.
// This method updates the selected state of the cell (in editing mode).
- (void)updateEditingStateOfCellAtIndexPath:(NSIndexPath*)indexPath
                      animateMenuVisibility:(BOOL)animateMenuVisibility
                       animateSelectedState:(BOOL)animateSelectedState {
  BookmarkCell* cell = base::mac::ObjCCast<BookmarkCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
  if (!cell)
    return;

  [self updateEditingStateOfCell:cell
                     atIndexPath:indexPath
           animateMenuVisibility:animateMenuVisibility
            animateSelectedState:animateSelectedState];
}

// The minimal horizontal space between items to respect between cells in
// |section|.
- (CGFloat)minimumInteritemSpacingForSectionAtIndex:(NSInteger)section {
  return 0;
}

// The minimal vertical space between items to respect between cells in
// |section|.
- (CGFloat)minimumLineSpacingForSectionAtIndex:(NSInteger)section {
  return 0;
}

// The text to display when there are no items in the collection. Default is
// |IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL|.
- (NSString*)textWhenCollectionIsEmpty {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL);
}

// In landscape mode, there are 2 widths: 480pt and 568pt. Returns YES if the
// width is 568pt.
- (BOOL)wideLandscapeMode {
  return self.frame.size.width > 567;
}

#pragma mark - Promo Cell

- (BOOL)isPromoSection:(NSInteger)section {
  return section == 0 && [self shouldShowPromoCell];
}

- (BOOL)shouldShowPromoCell {
  return _promoVisible;
}

- (BOOL)isPromoActive {
  return NO;
}

@end
