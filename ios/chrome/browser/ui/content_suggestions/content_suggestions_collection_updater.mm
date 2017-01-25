// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_article_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_expandable_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_stack_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsCollectionUpdater

@synthesize collectionViewController = _collectionViewController;

#pragma mark - Properties

- (void)setCollectionViewController:
    (ContentSuggestionsViewController*)collectionViewController {
  _collectionViewController = collectionViewController;
  [collectionViewController loadModel];
  CollectionViewModel* model = collectionViewController.collectionViewModel;
  NSInteger sectionIdentifier = kSectionIdentifierEnumZero;

  // Stack Item.
  [model addSectionWithIdentifier:sectionIdentifier];
  [model addItem:[[ContentSuggestionsStackItem alloc]
                     initWithType:ItemTypeStack
                            title:@"The title"
                         subtitle:@"The subtitle"]
      toSectionWithIdentifier:sectionIdentifier++];

  // Favicon Item.
  [model addSectionWithIdentifier:sectionIdentifier];
  ContentSuggestionsFaviconItem* faviconItem =
      [[ContentSuggestionsFaviconItem alloc] initWithType:ItemTypeFavicon];
  for (NSInteger i = 0; i < 6; i++) {
    [faviconItem addFavicon:[UIImage imageNamed:@"bookmark_gray_star"]
                  withTitle:@"Super website! Incredible!"];
  }
  faviconItem.delegate = _collectionViewController;
  [model addItem:faviconItem toSectionWithIdentifier:sectionIdentifier++];

  for (NSInteger i = 0; i < 3; i++) {
    [model addSectionWithIdentifier:sectionIdentifier];

    // Standard Item.
    [model addItem:[[ContentSuggestionsItem alloc] initWithType:ItemTypeText
                                                          title:@"The title"
                                                       subtitle:@"The subtitle"]
        toSectionWithIdentifier:sectionIdentifier];

    // Article Item.
    [model addItem:[[ContentSuggestionsArticleItem alloc]
                       initWithType:ItemTypeArticle
                              title:@"Title of an Article"
                           subtitle:@"This is the subtitle of an article, can "
                                    @"spawn on multiple lines"
                              image:[UIImage
                                        imageNamed:@"distillation_success"]]
        toSectionWithIdentifier:sectionIdentifier];

    // Expandable Item.
    SuggestionsExpandableItem* expandableItem =
        [[SuggestionsExpandableItem alloc]
            initWithType:ItemTypeExpand
                   title:@"Title of an Expandable Article"
                subtitle:@"This Article can be expanded to display "
                         @"additional information or interaction "
                         @"options"
                   image:[UIImage imageNamed:@"distillation_fail"]
              detailText:@"Details shown only when the article is "
                         @"expanded. It can be displayed on "
                         @"multiple lines."];
    expandableItem.delegate = _collectionViewController;
    [model addItem:expandableItem toSectionWithIdentifier:sectionIdentifier];
    sectionIdentifier++;
  }
}

#pragma mark - Public methods

- (void)addTextItem:(NSString*)title
           subtitle:(NSString*)subtitle
          toSection:(NSInteger)inputSection {
  DCHECK(_collectionViewController);
  ContentSuggestionsItem* item =
      [[ContentSuggestionsItem alloc] initWithType:ItemTypeText
                                             title:title
                                          subtitle:subtitle];
  NSInteger sectionIdentifier = kSectionIdentifierEnumZero + inputSection;
  NSInteger sectionIndex = inputSection;
  CollectionViewModel* model = _collectionViewController.collectionViewModel;
  if ([model numberOfSections] <= inputSection) {
    sectionIndex = [model numberOfSections];
    sectionIdentifier = kSectionIdentifierEnumZero + sectionIndex;
    [_collectionViewController.collectionView performBatchUpdates:^{
      [_collectionViewController.collectionViewModel
          addSectionWithIdentifier:sectionIdentifier];
      [_collectionViewController.collectionView
          insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]];
    }
                                                       completion:nil];
  }
  NSInteger numberOfItemsInSection =
      [model numberOfItemsInSection:sectionIndex];
  [_collectionViewController.collectionViewModel addItem:item
                                 toSectionWithIdentifier:sectionIdentifier];
  [_collectionViewController.collectionView performBatchUpdates:^{
    [_collectionViewController.collectionView
        insertItemsAtIndexPaths:@[ [NSIndexPath
                                    indexPathForRow:numberOfItemsInSection
                                          inSection:sectionIndex] ]];
  }
                                                     completion:nil];
}

- (BOOL)shouldUseCustomStyleForSection:(NSInteger)section {
  return section == 0 || section == 1;
}

@end
