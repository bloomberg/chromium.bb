// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/suggestions/suggestions_collection_updater.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_article_item.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_expandable_item.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_item.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeText = kItemTypeEnumZero,
  ItemTypeArticle,
  ItemTypeExpand,
};

}  // namespace

@implementation SuggestionsCollectionUpdater {
  SuggestionsViewController* _collectionViewController;
}

- (instancetype)initWithCollectionViewController:
    (SuggestionsViewController*)collectionViewController {
  self = [super init];
  if (self) {
    _collectionViewController = collectionViewController;
    [collectionViewController loadModel];
    CollectionViewModel* model = collectionViewController.collectionViewModel;
    NSInteger sectionIdentifier = kSectionIdentifierEnumZero;
    for (NSInteger i = 0; i < 3; i++) {
      [model addSectionWithIdentifier:sectionIdentifier];
      [model addItem:[[SuggestionsItem alloc] initWithType:ItemTypeText
                                                     title:@"The title"
                                                  subtitle:@"The subtitle"]
          toSectionWithIdentifier:sectionIdentifier];
      [model addItem:
                 [[SuggestionsArticleItem alloc]
                     initWithType:ItemTypeArticle
                            title:@"Title of an Article"
                         subtitle:@"This is the subtitle of an article, can "
                                  @"spawn on multiple lines"
                            image:[UIImage imageNamed:@"distillation_success"]]
          toSectionWithIdentifier:sectionIdentifier];
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
      expandableItem.delegate = collectionViewController;
      [model addItem:expandableItem toSectionWithIdentifier:sectionIdentifier];
      sectionIdentifier++;
    }
  }
  return self;
}

#pragma mark - Public methods

- (void)addTextItem:(NSString*)title
           subtitle:(NSString*)subtitle
          toSection:(NSInteger)inputSection {
  SuggestionsItem* item = [[SuggestionsItem alloc] initWithType:ItemTypeText
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

@end
