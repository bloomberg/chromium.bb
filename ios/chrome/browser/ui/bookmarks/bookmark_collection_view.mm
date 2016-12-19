// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view.h"

#import <UIKit/UIGestureRecognizerSubclass.h>
#include <algorithm>
#include <map>
#include <memory>

#include "base/ios/weak_nsobject.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_cells.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view_background.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_cell.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/l10n/l10n_util_mac.h"

using bookmarks::BookmarkNode;

namespace {

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

}  // namespace

@interface BookmarkCollectionView ()<UICollectionViewDataSource,
                                     UICollectionViewDelegateFlowLayout,
                                     UIGestureRecognizerDelegate> {
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
  ios::ChromeBrowserState* _browserState;

  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkCollectionView;

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
@property(nonatomic, retain) UICollectionView* collectionView;
// Redefined to be readwrite.
@property(nonatomic, assign) BOOL editing;
// Detects a long press on a cell.
@property(nonatomic, retain) UILongPressGestureRecognizer* longPressRecognizer;
// Background view of the collection view shown when there is no items.
@property(nonatomic, retain)
    BookmarkCollectionViewBackground* emptyCollectionBackgroundView;
// Shadow to display over the content.
@property(nonatomic, retain) UIView* shadow;

// Updates the editing state for the cell.
- (void)updateEditingStateOfCell:(BookmarkCell*)cell
                     atIndexPath:(NSIndexPath*)indexPath
           animateMenuVisibility:(BOOL)animateMenuVisibility
            animateSelectedState:(BOOL)animateSelectedState;

// Callback received when the user taps the menu button on the cell.
- (void)didTapMenuButton:(BookmarkItemCell*)cell view:(UIView*)view;

// In landscape mode, there are 2 widths: 480pt and 568pt. Returns YES if the
// width is 568pt.
- (BOOL)wideLandscapeMode;

// Schedules showing or hiding the empty bookmarks background view if the
// collection view is empty by calling showEmptyBackgroundIfNeeded after
// kShowEmptyBookmarksBackgroundRefreshDelay.
// Multiple call to this method will cancel previous scheduled call to
// showEmptyBackgroundIfNeeded before scheduling a new one.
- (void)scheduleEmptyBackgroundVisibilityUpdate;
// Shows/hides empty bookmarks background view if the collections view is empty.
- (void)updateEmptyBackgroundVisibility;
// Shows/hides empty bookmarks background view with an animation.
- (void)setEmptyBackgroundVisible:(BOOL)visible;
@end

@implementation BookmarkCollectionView
@synthesize bookmarkModel = _bookmarkModel;
@synthesize collectionView = _collectionView;
@synthesize editing = _editing;
@synthesize emptyCollectionBackgroundView = _emptyCollectionBackgroundView;
@synthesize loader = _loader;
@synthesize longPressRecognizer = _longPressRecognizer;
@synthesize browserState = _browserState;
@synthesize shadow = _shadow;

#pragma mark - Initialization

- (id)init {
  NOTREACHED();
  return nil;
}

- (id)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                               frame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkCollectionView.Init(
        self, [BookmarkCollectionView class]);

    _browserState = browserState;

    // Set up connection to the BookmarkModel.
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(browserState);

    // Set up observers.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));

    [self setupViews];
  }
  return self;
}

- (void)dealloc {
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
  [super dealloc];
}

- (void)setupViews {
  self.backgroundColor = bookmark_utils_ios::mainBackgroundColor();
  base::scoped_nsobject<UICollectionViewFlowLayout> layout(
      [[UICollectionViewFlowLayout alloc] init]);

  base::scoped_nsobject<UICollectionView> collectionView(
      [[UICollectionView alloc] initWithFrame:self.bounds
                         collectionViewLayout:layout]);
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

  [self addSubview:self.collectionView];

  // Set up the background view shown when the collection is empty.
  base::scoped_nsobject<BookmarkCollectionViewBackground>
      emptyCollectionBackgroundView(
          [[BookmarkCollectionViewBackground alloc] initWithFrame:CGRectZero]);
  self.emptyCollectionBackgroundView = emptyCollectionBackgroundView;
  self.emptyCollectionBackgroundView.autoresizingMask =
      UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
  self.emptyCollectionBackgroundView.alpha = 0;
  self.emptyCollectionBackgroundView.text = [self textWhenCollectionIsEmpty];

  self.emptyCollectionBackgroundView.frame = self.collectionView.bounds;
  self.collectionView.backgroundView = self.emptyCollectionBackgroundView;

  [self updateShadow];

  self.longPressRecognizer =
      base::scoped_nsobject<UILongPressGestureRecognizer>(
          [[UILongPressGestureRecognizer alloc]
              initWithTarget:self
                      action:@selector(longPress:)]);
  self.longPressRecognizer.delegate = self;
  [self.collectionView addGestureRecognizer:self.longPressRecognizer];
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
    self.shadow = [[[UIView alloc]
        initWithFrame:CGRectMake(0, 0, CGRectGetWidth(self.bounds),
                                 1 / [[UIScreen mainScreen] scale])]
        autorelease];
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

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateShadowFrame];
  [self collectionViewScrolled];
}

#pragma mark - empty background

- (void)scheduleEmptyBackgroundVisibilityUpdate {
  [NSObject
      cancelPreviousPerformRequestsWithTarget:self
                                     selector:
                                         @selector(
                                             updateEmptyBackgroundVisibility)
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

- (void)updateEmptyBackgroundVisibility {
  const BOOL showEmptyBackground =
      [self isCollectionViewEmpty] && ![self shouldShowPromoCell];
  [self setEmptyBackgroundVisible:showEmptyBackground];
}

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

#pragma mark - BookmarkItemCell callbacks

- (void)didTapMenuButton:(BookmarkItemCell*)cell view:(UIView*)view {
  [self didTapMenuButtonAtIndexPath:[self.collectionView indexPathForCell:cell]
                             onView:view
                            forCell:cell];
}

#pragma mark - Convenience methods for subclasses

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

- (void)cancelAllFaviconLoads {
  _faviconTaskTracker.TryCancelAll();
}

- (void)cancelLoadingFaviconAtIndexPath:(NSIndexPath*)indexPath {
  _faviconTaskTracker.TryCancel(
      _faviconLoadTasks[IntegerPair(indexPath.section, indexPath.item)]);
}

- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath {
  // Cancel previous load attempts.
  [self cancelLoadingFaviconAtIndexPath:indexPath];

  // Start loading a favicon.
  base::WeakNSObject<BookmarkCollectionView> weakSelf(self);
  const bookmarks::BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  GURL blockURL(node->url());
  void (^faviconBlock)(const favicon_base::LargeIconResult&) = ^(
      const favicon_base::LargeIconResult& result) {
    base::scoped_nsobject<BookmarkCollectionView> strongSelf([weakSelf retain]);
    if (!strongSelf)
      return;
    UIImage* favIcon = nil;
    UIColor* backgroundColor = nil;
    UIColor* textColor = nil;
    NSString* fallbackText = nil;
    if (result.bitmap.is_valid()) {
      scoped_refptr<base::RefCountedMemory> data =
          result.bitmap.bitmap_data.get();
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
                                        base::BindBlock(faviconBlock),
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

#pragma mark - BookmarkModelObserver Callbacks

- (void)bookmarkModelLoaded {
  NOTREACHED();
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  NOTREACHED();
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  NOTREACHED();
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  NOTREACHED();
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  NOTREACHED();
}

- (void)bookmarkModelRemovedAllNodes {
  NOTREACHED();
}

#pragma mark - Public Methods That Must Be Overridden

- (BOOL)shouldSelectCellAtIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
  return NO;
}

- (void)didTapCellAtIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
}

- (void)didAddCellForEditingAtIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
}

- (void)didRemoveCellForEditingAtIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
}

- (void)didTapMenuButtonAtIndexPath:(NSIndexPath*)indexPath
                             onView:(UIView*)view
                            forCell:(BookmarkItemCell*)cell {
  NOTREACHED();
}

- (bookmark_cell::ButtonType)buttonTypeForCellAtIndexPath:
    (NSIndexPath*)indexPath {
  NOTREACHED();
  return bookmark_cell::ButtonNone;
}

- (BOOL)allowLongPressForCellAtIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
  return NO;
}

- (void)didLongPressCell:(UICollectionViewCell*)cell
             atIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
}

- (BOOL)cellIsSelectedForEditingAtIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
  return NO;
}

- (CGSize)headerSizeForSection:(NSInteger)section {
  NOTREACHED();
  return CGSizeZero;
}

- (UICollectionViewCell*)cellAtIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
  return nil;
}

- (UICollectionReusableView*)headerAtIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
  return nil;
}

- (NSInteger)numberOfItemsInSection:(NSInteger)section {
  NOTREACHED();
  return 0;
}
- (NSInteger)numberOfSections {
  NOTREACHED();
  return 0;
}

- (void)updateCollectionView {
  NOTREACHED();
}

- (const bookmarks::BookmarkNode*)nodeAtIndexPath:(NSIndexPath*)indexPath {
  NOTREACHED();
  return nullptr;
}

#pragma mark - Methods that subclasses can override (UI)

- (UIEdgeInsets)insetForSectionAtIndex:(NSInteger)section {
  if ([self isPromoSection:section])
    return UIEdgeInsetsZero;

  if (IsIPadIdiom()) {
    return UIEdgeInsetsMake(10, rowMarginTablet, 0, rowMarginTablet);
  } else {
    return UIEdgeInsetsZero;
  }
}

- (CGSize)cellSizeForIndexPath:(NSIndexPath*)indexPath {
  if ([self isPromoSection:indexPath.section]) {
    CGRect estimatedFrame = CGRectMake(0, 0, CGRectGetWidth(self.bounds), 100);
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    if (!cell) {
      cell = [[[BookmarkPromoCell alloc] initWithFrame:estimatedFrame]
          autorelease];
    }
    cell.frame = estimatedFrame;
    [cell layoutIfNeeded];
    return [cell systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
  }

  UIEdgeInsets insets = [self insetForSectionAtIndex:indexPath.section];
  return CGSizeMake(self.bounds.size.width - (insets.right + insets.left),
                    rowHeight);
}

- (CGFloat)minimumInteritemSpacingForSectionAtIndex:(NSInteger)section {
  return 0;
}

- (CGFloat)minimumLineSpacingForSectionAtIndex:(NSInteger)section {
  return 0;
}

- (NSString*)textWhenCollectionIsEmpty {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL);
}

#pragma mark - Public Methods That Can Be Overridden

- (void)collectionViewScrolled {
}

#pragma mark - Editing

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
}

#pragma mark - Public Methods

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

- (void)setScrollsToTop:(BOOL)scrollsToTop {
  self.collectionView.scrollsToTop = scrollsToTop;
}

#pragma mark - Private Methods

- (BOOL)wideLandscapeMode {
  return self.frame.size.width > 567;
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

#pragma mark - Promo Cell

- (BOOL)isPromoSection:(NSInteger)section {
  return section == 0 && [self shouldShowPromoCell];
}

- (BOOL)shouldShowPromoCell {
  return NO;
}

- (BOOL)isPromoActive {
  return NO;
}

@end
