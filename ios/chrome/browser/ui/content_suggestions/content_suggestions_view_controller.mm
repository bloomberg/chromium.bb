// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

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

// Left and right margins for the content inset of the collection view.
@property(nonatomic, assign) CGFloat cardStyleMargin;

@end

@implementation ContentSuggestionsViewController

@synthesize suggestionCommandHandler = _suggestionCommandHandler;
@synthesize collectionUpdater = _collectionUpdater;
@synthesize cardStyleMargin = _cardStyleMargin;
@dynamic collectionViewModel;

#pragma mark - Public

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
                   dataSource:(id<ContentSuggestionsDataSource>)dataSource {
  self = [super initWithStyle:style];
  if (self) {
    _collectionUpdater = [[ContentSuggestionsCollectionUpdater alloc]
        initWithDataSource:dataSource];
  }
  return self;
}

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
        // The context menu could be displayed for the deleted entry.
        [self.suggestionCommandHandler dismissContextMenu];
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
        // The context menu could be displayed for the deleted entries.
        [self.suggestionCommandHandler dismissContextMenu];
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
                                completion:nil];

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
                                completion:nil];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _collectionUpdater.collectionViewController = self;

  self.collectionView.delegate = self;
  self.collectionView.backgroundColor = [UIColor whiteColor];
  if (ShouldCellsBeFullWidth(
          [UIApplication sharedApplication].keyWindow.traitCollection)) {
    self.styler.cellStyle = MDCCollectionViewCellStyleGrouped;
  } else {
    self.styler.cellStyle = MDCCollectionViewCellStyleCard;
    CGFloat margin =
        MAX(0, (self.collectionView.frame.size.width - kMaxCardWidth) / 2);
    self.collectionView.contentInset = UIEdgeInsetsMake(0, margin, 0, margin);
  }
  self.automaticallyAdjustsScrollViewInsets = NO;
  self.collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  ApplyVisualConstraints(
      @[ @"V:[top][collection]|", @"H:|[collection]|" ],
      @{ @"collection" : self.collectionView,
         @"top" : self.topLayoutGuide });

  UILongPressGestureRecognizer* longPressRecognizer =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  longPressRecognizer.numberOfTouchesRequired = 1;
  [self.collectionView addGestureRecognizer:longPressRecognizer];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  // Change the margin, in case -willTransitionToTraitCollection: is called
  // after this method.
  self.cardStyleMargin = MAX(0, (size.width - kMaxCardWidth) / 2);
  if (self.styler.cellStyle == MDCCollectionViewCellStyleCard) {
    self.collectionView.contentInset =
        UIEdgeInsetsMake(0, self.cardStyleMargin, 0, self.cardStyleMargin);
  }
  [self.collectionUpdater updateMostVisitedForSize:size];
}

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  if (ShouldCellsBeFullWidth(newCollection)) {
    self.collectionView.contentInset = UIEdgeInsetsZero;
    self.styler.cellStyle = MDCCollectionViewCellStyleGrouped;
  } else {
    self.collectionView.contentInset =
        UIEdgeInsetsMake(0, self.cardStyleMargin, 0, self.cardStyleMargin);
    self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  }
  [self.collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch ([self.collectionUpdater contentSuggestionTypeForItem:item]) {
    case ContentSuggestionTypeReadingList:
    case ContentSuggestionTypeArticle:
      [self unfocusOmnibox];
      [self.suggestionCommandHandler openPageForItem:item];
      break;
    case ContentSuggestionTypeMostVisited:
      [self unfocusOmnibox];
      [self.suggestionCommandHandler openMostVisitedItem:item
                                                 atIndex:indexPath.item];
      break;
    case ContentSuggestionTypePromo:
      [self unfocusOmnibox];
      [self dismissEntryAtIndexPath:indexPath];
      [self.suggestionCommandHandler handlePromoTapped];
      [self.collectionViewLayout invalidateLayout];
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
  if ([self.collectionUpdater isMostVisitedSection:section] ||
      [self.collectionUpdater isHeaderSection:section]) {
    CGFloat margin = content_suggestions::centeredTilesMarginForWidth(
        collectionView.frame.size.width - 2 * collectionView.contentInset.left);
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
  // TODO(crbug.com/635604): Once the headers support dynamic sizing, use it
  // instead of this.
  if ([self.collectionUpdater isHeaderSection:section])
    return CGSizeMake(0, 270);
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
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];
  UIEdgeInsets contentInset = self.collectionView.contentInset;

  return [MDCCollectionViewCell
      cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds) -
                                 inset.left - inset.right - contentInset.left -
                                 contentInset.right
                         forItem:item];
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
             ContentSuggestionTypeEmpty;
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndSwipeToDismissItemAtIndexPath:(NSIndexPath*)indexPath {
  [self.collectionUpdater
      dismissItem:[self.collectionViewModel itemAtIndexPath:indexPath]];
  [self dismissEntryAtIndexPath:indexPath];
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

// Tells WebToolbarController to resign focus to the omnibox.
- (void)unfocusOmnibox {
  // TODO(crbug.com/700375): once the omnibox is part of Content Suggestions,
  // remove the fake omnibox focus here.
}

@end
