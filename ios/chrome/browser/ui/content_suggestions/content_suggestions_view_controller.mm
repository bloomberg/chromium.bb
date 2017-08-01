// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_delegate.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
const CGFloat kMaxCardWidth = 432;

// Returns whether the cells should be displayed using the full width.
BOOL ShouldCellsBeFullWidth(UITraitCollection* collection) {
  return collection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
         collection.verticalSizeClass != UIUserInterfaceSizeClassCompact;
}
}

@interface ContentSuggestionsViewController ()

@property(nonatomic, strong)
    ContentSuggestionsCollectionUpdater* collectionUpdater;

// The overscroll actions controller managing accelerators over the toolbar.
@property(nonatomic, strong)
    OverscrollActionsController* overscrollActionsController;
@end

@implementation ContentSuggestionsViewController

@synthesize audience = _audience;
@synthesize suggestionCommandHandler = _suggestionCommandHandler;
@synthesize headerCommandHandler = _headerCommandHandler;
@synthesize suggestionsDelegate = _suggestionsDelegate;
@synthesize collectionUpdater = _collectionUpdater;
@synthesize overscrollActionsController = _overscrollActionsController;
@synthesize overscrollDelegate = _overscrollDelegate;
@synthesize scrolledToTop = _scrolledToTop;
@dynamic collectionViewModel;

#pragma mark - Lifecycle

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
                   dataSource:(id<ContentSuggestionsDataSource>)dataSource {
  UICollectionViewLayout* layout = nil;
  if (IsIPadIdiom()) {
    layout = [[MDCCollectionViewFlowLayout alloc] init];
  } else {
    layout = [[ContentSuggestionsLayout alloc] init];
  }
  self = [super initWithLayout:layout style:style];
  if (self) {
    _collectionUpdater = [[ContentSuggestionsCollectionUpdater alloc]
        initWithDataSource:dataSource];
  }
  return self;
}

- (void)dealloc {
  [self.overscrollActionsController invalidate];
}

#pragma mark - Public

- (void)dismissEntryAtIndexPath:(NSIndexPath*)indexPath {
  if (!indexPath || ![self.collectionViewModel hasItemAtIndexPath:indexPath]) {
    return;
  }

  [self.collectionView performBatchUpdates:^{
    [self collectionView:self.collectionView
        willDeleteItemsAtIndexPaths:@[ indexPath ]];

    [self.collectionView deleteItemsAtIndexPaths:@[ indexPath ]];

    // Check if the section is now empty.
    [self addEmptySectionPlaceholderIfNeeded:indexPath.section];
  }
      completion:^(BOOL) {
        [self.audience contentSuggestionsDidScroll];
        // The context menu could be displayed for the deleted entry.
        [self.suggestionCommandHandler dismissModals];
      }];
}

- (void)dismissSection:(NSInteger)section {
  if (section >= [self numberOfSectionsInCollectionView:self.collectionView]) {
    return;
  }

  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];

  [self.collectionView performBatchUpdates:^{
    [self.collectionViewModel removeSectionWithIdentifier:sectionIdentifier];
    [self.collectionView deleteSections:[NSIndexSet indexSetWithIndex:section]];
  }
      completion:^(BOOL) {
        [self.audience contentSuggestionsDidScroll];
        // The context menu could be displayed for the deleted entries.
        [self.suggestionCommandHandler dismissModals];
      }];
}

- (void)addSuggestions:(NSArray<CSCollectionViewItem*>*)suggestions
         toSectionInfo:(ContentSuggestionsSectionInformation*)sectionInfo {
  if (suggestions.count == 0) {
    return;
  }

  [self.collectionView performBatchUpdates:^{
    NSIndexSet* addedSections = [self.collectionUpdater
        addSectionsForSectionInfoToModel:@[ sectionInfo ]];
    [self.collectionView insertSections:addedSections];
  }
      completion:^(BOOL) {
        [self.audience contentSuggestionsDidScroll];
      }];

  [self.collectionView performBatchUpdates:^{
    NSIndexPath* removedItem = [self.collectionUpdater
        removeEmptySuggestionsForSectionInfo:sectionInfo];
    if (removedItem) {
      [self.collectionView deleteItemsAtIndexPaths:@[ removedItem ]];
    }

    NSArray<NSIndexPath*>* addedItems =
        [self.collectionUpdater addSuggestionsToModel:suggestions
                                      withSectionInfo:sectionInfo];
    [self.collectionView insertItemsAtIndexPaths:addedItems];
  }
      completion:^(BOOL) {
        [self.audience contentSuggestionsDidScroll];
      }];
}

+ (NSString*)collectionAccessibilityIdentifier {
  return @"ContentSuggestionsCollectionIdentifier";
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  if (base::ios::IsRunningOnIOS10OrLater()) {
    self.collectionView.prefetchingEnabled = NO;
  }
  self.collectionView.accessibilityIdentifier =
      [[self class] collectionAccessibilityIdentifier];
  _collectionUpdater.collectionViewController = self;

  self.collectionView.delegate = self;
  self.collectionView.backgroundColor = [UIColor whiteColor];
  if (ShouldCellsBeFullWidth(
          [UIApplication sharedApplication].keyWindow.traitCollection)) {
    self.styler.cellStyle = MDCCollectionViewCellStyleGrouped;
  } else {
    self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  }
  self.automaticallyAdjustsScrollViewInsets = NO;
  self.collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  ApplyVisualConstraints(@[ @"V:|[collection]|", @"H:|[collection]|" ],
                         @{@"collection" : self.collectionView});

  UILongPressGestureRecognizer* longPressRecognizer =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  longPressRecognizer.numberOfTouchesRequired = 1;
  [self.collectionView addGestureRecognizer:longPressRecognizer];

  if (!IsIPadIdiom()) {
    self.overscrollActionsController = [[OverscrollActionsController alloc]
        initWithScrollView:self.collectionView];
    [self.overscrollActionsController
        setStyle:OverscrollStyle::NTP_NON_INCOGNITO];
    self.overscrollActionsController.delegate = self.overscrollDelegate;
  }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self.collectionUpdater updateMostVisitedForSize:size];
  [self.collectionView reloadData];

  void (^alongsideBlock)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.headerCommandHandler
            updateFakeOmniboxForScrollView:self.collectionView];
      };
  [coordinator animateAlongsideTransition:alongsideBlock completion:nil];
}

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super willTransitionToTraitCollection:newCollection
               withTransitionCoordinator:coordinator];
  // Invalidating the layout after changing the cellStyle results in the layout
  // not being updated. Do it before to have it taken into account.
  [self.collectionView.collectionViewLayout invalidateLayout];
  if (ShouldCellsBeFullWidth(newCollection)) {
    self.styler.cellStyle = MDCCollectionViewCellStyleGrouped;
  } else {
    self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  }
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  [self.headerCommandHandler unfocusOmnibox];

  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch ([self.collectionUpdater contentSuggestionTypeForItem:item]) {
    case ContentSuggestionTypeReadingList:
      base::RecordAction(base::UserMetricsAction("MobileReadingListOpen"));
      [self.suggestionCommandHandler openPageForItem:item];
      break;
    case ContentSuggestionTypeArticle:
      [self.suggestionCommandHandler openPageForItem:item];
      break;
    case ContentSuggestionTypeMostVisited:
      [self.suggestionCommandHandler openMostVisitedItem:item
                                                 atIndex:indexPath.item];
      break;
    case ContentSuggestionTypePromo:
      [self dismissEntryAtIndexPath:indexPath];
      [self.suggestionCommandHandler handlePromoTapped];
      [self.collectionViewLayout invalidateLayout];
      break;
    case ContentSuggestionTypeLearnMore:
      [self.suggestionCommandHandler handleLearnMoreTapped];
      break;
    case ContentSuggestionTypeEmpty:
      break;
  }
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([self.collectionUpdater isMostVisitedSection:indexPath.section]) {
    return [ContentSuggestionsMostVisitedCell defaultSize];
  } else if ([self.collectionUpdater isHeaderSection:indexPath.section]) {
    CGFloat height =
        [self collectionView:collectionView cellHeightAtIndexPath:indexPath];
    CGFloat width = collectionView.frame.size.width -
                    2 * content_suggestions::centeredTilesMarginForWidth(
                            collectionView.frame.size.width);
    return CGSizeMake(width, height);
  }
  return [super collectionView:collectionView
                        layout:collectionViewLayout
        sizeForItemAtIndexPath:indexPath];
}

- (UIEdgeInsets)collectionView:(UICollectionView*)collectionView
                        layout:(UICollectionViewLayout*)collectionViewLayout
        insetForSectionAtIndex:(NSInteger)section {
  UIEdgeInsets parentInset = [super collectionView:collectionView
                                            layout:collectionViewLayout
                            insetForSectionAtIndex:section];
  if ([self.collectionUpdater isHeaderSection:section]) {
    return parentInset;
  }
  if ([self.collectionUpdater isMostVisitedSection:section]) {
    CGFloat margin = content_suggestions::centeredTilesMarginForWidth(
        collectionView.frame.size.width);
    parentInset.left = margin;
    parentInset.right = margin;
  } else if (self.styler.cellStyle == MDCCollectionViewCellStyleCard) {
    CGFloat margin =
        MAX(0, (collectionView.frame.size.width - kMaxCardWidth) / 2);
    parentInset.left = margin;
    parentInset.right = margin;
  }
  return parentInset;
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
                                 layout:(UICollectionViewLayout*)
                                            collectionViewLayout
    minimumLineSpacingForSectionAtIndex:(NSInteger)section {
  if ([self.collectionUpdater isMostVisitedSection:section]) {
    return content_suggestions::spacingBetweenTiles();
  }
  return [super collectionView:collectionView
                                   layout:collectionViewLayout
      minimumLineSpacingForSectionAtIndex:section];
}

#pragma mark - MDCCollectionViewStylingDelegate

// TODO(crbug.com/724493): Use collectionView:hidesInkViewAtIndexPath: when it
// is fixed. For now hidding the ink prevent cell interaction.
- (UIColor*)collectionView:(UICollectionView*)collectionView
       inkColorAtIndexPath:(NSIndexPath*)indexPath {
  ContentSuggestionType itemType = [self.collectionUpdater
      contentSuggestionTypeForItem:[self.collectionViewModel
                                       itemAtIndexPath:indexPath]];
  if ([self.collectionUpdater isMostVisitedSection:indexPath.section] ||
      itemType == ContentSuggestionTypeEmpty) {
    return [UIColor clearColor];
  }
  return nil;
}

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  if ([self.collectionUpdater isHeaderSection:section]) {
    return MDCCollectionViewCellStyleDefault;
  }
  return [super collectionView:collectionView cellStyleForSection:section];
}

- (UIColor*)collectionView:(nonnull UICollectionView*)collectionView
    cellBackgroundColorAtIndexPath:(nonnull NSIndexPath*)indexPath {
  if ([self.collectionUpdater
          shouldUseCustomStyleForSection:indexPath.section]) {
    return [UIColor clearColor];
  }
  return [UIColor whiteColor];
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  if ([self.collectionUpdater isHeaderSection:section]) {
    return CGSizeMake(0, [self.suggestionsDelegate headerHeight]);
  }
  return [super collectionView:collectionView
                               layout:collectionViewLayout
      referenceSizeForHeaderInSection:section];
}

- (BOOL)collectionView:(nonnull UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(nonnull NSIndexPath*)indexPath {
  return
      [self.collectionUpdater shouldUseCustomStyleForSection:indexPath.section];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideHeaderBackgroundForSection:(NSInteger)section {
  return YES;
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CSCollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];
  CGFloat width =
      CGRectGetWidth(collectionView.bounds) - inset.left - inset.right;

  return [item cellHeightForWidth:width];
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsSwipeToDismissItem:
    (UICollectionView*)collectionView {
  return YES;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canSwipeToDismissItemAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  return ![self.collectionUpdater isMostVisitedSection:indexPath.section] &&
         ![self.collectionUpdater isHeaderSection:indexPath.section] &&
         [self.collectionUpdater contentSuggestionTypeForItem:item] !=
             ContentSuggestionTypeLearnMore &&
         [self.collectionUpdater contentSuggestionTypeForItem:item] !=
             ContentSuggestionTypeEmpty;
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndSwipeToDismissItemAtIndexPath:(NSIndexPath*)indexPath {
  [self.collectionUpdater
      dismissItem:[self.collectionViewModel itemAtIndexPath:indexPath]];
  [self dismissEntryAtIndexPath:indexPath];
}

#pragma mark - UIScrollViewDelegate Methods.

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];
  [self.audience contentSuggestionsDidScroll];
  [self.overscrollActionsController scrollViewDidScroll:scrollView];
  [self.headerCommandHandler updateFakeOmniboxForScrollView:scrollView];
  self.scrolledToTop =
      scrollView.contentOffset.y >= [self.suggestionsDelegate pinnedOffsetY];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [self.overscrollActionsController scrollViewWillBeginDragging:scrollView];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  [super scrollViewDidEndDragging:scrollView willDecelerate:decelerate];
  [self.overscrollActionsController scrollViewDidEndDragging:scrollView
                                              willDecelerate:decelerate];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  [super scrollViewWillEndDragging:scrollView
                      withVelocity:velocity
               targetContentOffset:targetContentOffset];
  [self.overscrollActionsController
      scrollViewWillEndDragging:scrollView
                   withVelocity:velocity
            targetContentOffset:targetContentOffset];
}

#pragma mark - Private

- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer {
  if (self.editor.editing ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }

  CGPoint touchLocation =
      [gestureRecognizer locationOfTouch:0 inView:self.collectionView];
  NSIndexPath* touchedItemIndexPath =
      [self.collectionView indexPathForItemAtPoint:touchLocation];
  if (!touchedItemIndexPath ||
      ![self.collectionViewModel hasItemAtIndexPath:touchedItemIndexPath]) {
    // Make sure there is an item at this position.
    return;
  }
  CollectionViewItem* touchedItem =
      [self.collectionViewModel itemAtIndexPath:touchedItemIndexPath];

  ContentSuggestionType type =
      [self.collectionUpdater contentSuggestionTypeForItem:touchedItem];
  switch (type) {
    case ContentSuggestionTypeArticle:
      [self.suggestionCommandHandler
          displayContextMenuForArticle:touchedItem
                               atPoint:touchLocation
                           atIndexPath:touchedItemIndexPath];
      break;
    case ContentSuggestionTypeMostVisited:
      [self.suggestionCommandHandler
          displayContextMenuForMostVisitedItem:touchedItem
                                       atPoint:touchLocation
                                   atIndexPath:touchedItemIndexPath];
      break;
    default:
      break;
  }

}

// Checks if the |section| is empty and add an empty element if it is the case.
// Must be called from inside a performBatchUpdates: block.
- (void)addEmptySectionPlaceholderIfNeeded:(NSInteger)section {
  if ([self.collectionViewModel numberOfItemsInSection:section] > 0)
    return;

  NSIndexPath* emptyItem =
      [self.collectionUpdater addEmptyItemForSection:section];
  if (emptyItem)
    [self.collectionView insertItemsAtIndexPaths:@[ emptyItem ]];
}

@end
