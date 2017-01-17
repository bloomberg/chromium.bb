// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/suggestions/suggestions_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/suggestions/expandable_item.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_collection_updater.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_commands.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_item_actions.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_stack_item.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_stack_item_actions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const NSTimeInterval kAnimationDuration = 0.35;
}  // namespace

@interface SuggestionsViewController ()<SuggestionsItemActions,
                                        SuggestionsStackItemActions>

@property(nonatomic, strong) SuggestionsCollectionUpdater* collectionUpdater;

// Expand or collapse the |cell|, if it is a SuggestionsExpandableCell,
// according to |expand|.
- (void)expand:(BOOL)expand cell:(UICollectionViewCell*)cell;

@end

@implementation SuggestionsViewController

@synthesize suggestionCommandHandler = _suggestionCommandHandler;
@synthesize collectionUpdater = _collectionUpdater;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _collectionUpdater = [[SuggestionsCollectionUpdater alloc] init];
  _collectionUpdater.collectionViewController = self;

  self.collectionView.delegate = self;
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if (item.type == ItemTypeStack) {
    [self.suggestionCommandHandler openReadingList];
  }
}

#pragma mark - SuggestionsExpandableCellDelegate

- (void)collapseCell:(UICollectionViewCell*)cell {
  [self expand:NO cell:cell];
}

- (void)expandCell:(UICollectionViewCell*)cell {
  [self expand:YES cell:cell];
}

#pragma mark - SuggestionsItemActions

- (void)addNewItem:(id)sender {
  [self.suggestionCommandHandler addEmptyItem];
}

#pragma mark - SuggestionsCollectionUpdater forwarding

- (void)addTextItem:(NSString*)title
           subtitle:(NSString*)subtitle
          toSection:(NSInteger)inputSection {
  [self.collectionUpdater addTextItem:title
                             subtitle:subtitle
                            toSection:inputSection];
}

#pragma mark - SuggestionsStackItemActions

- (void)openReadingListFirstItem:(id)sender {
  [self.suggestionCommandHandler openFirstPageOfReadingList];
}

#pragma mark - MDCCollectionViewStylingDelegate

- (UIColor*)collectionView:(nonnull UICollectionView*)collectionView
    cellBackgroundColorAtIndexPath:(nonnull NSIndexPath*)indexPath {
  if ([self.collectionUpdater
          shouldUseCustomStyleForSection:indexPath.section]) {
    return [UIColor clearColor];
  }
  return [UIColor whiteColor];
}

- (BOOL)collectionView:(nonnull UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(nonnull NSIndexPath*)indexPath {
  if ([self.collectionUpdater
          shouldUseCustomStyleForSection:indexPath.section]) {
    return YES;
  }
  return NO;
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];

  return [MDCCollectionViewCell
      cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds) -
                                 inset.left - inset.right
                         forItem:item];
}

#pragma mark - Private

- (void)expand:(BOOL)expand cell:(UICollectionViewCell*)cell {
  NSIndexPath* indexPath = [self.collectionView indexPathForCell:cell];
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if ([item conformsToProtocol:@protocol(SuggestionsExpandableArticle)]) {
    id<SuggestionsExpandableArticle> expandableItem =
        (id<SuggestionsExpandableArticle>)item;

    NSInteger sectionIdentifier = [self.collectionViewModel
        sectionIdentifierForSection:indexPath.section];

    expandableItem.expanded = expand;
    [self reconfigureCellsForItems:@[ item ]
           inSectionWithIdentifier:sectionIdentifier];

    [UIView
        animateWithDuration:kAnimationDuration
                 animations:^{
                   [self.collectionView.collectionViewLayout invalidateLayout];
                 }];
  }
}

@end
