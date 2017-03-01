// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestion.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_article_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_expandable_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_fetcher.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_stack_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_text_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Enum defining the ItemType of this ContentSuggestionsCollectionUpdater.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeText = kItemTypeEnumZero,
  ItemTypeArticle,
  ItemTypeExpand,
  ItemTypeStack,
  ItemTypeFavicon,
};

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierBookmarks = kSectionIdentifierEnumZero,
  SectionIdentifierArticles,
  SectionIdentifierDefault,
};

ItemType ItemTypeForContentSuggestionType(ContentSuggestionType type) {
  switch (type) {
    case ContentSuggestionTypeArticle:
      return ItemTypeArticle;
  }
}

ContentSuggestionType ContentSuggestionTypeForItemType(NSInteger type) {
  if (type == ItemTypeArticle)
    return ContentSuggestionTypeArticle;
  // Add new type here

  // Default type.
  return ContentSuggestionTypeArticle;
}

// Returns the section identifier corresponding to the section |info|.
SectionIdentifier SectionIdentifierForInfo(
    ContentSuggestionsSectionInformation* info) {
  switch (info.sectionID) {
    case ContentSuggestionsSectionBookmarks:
      return SectionIdentifierBookmarks;

    case ContentSuggestionsSectionArticles:
      return SectionIdentifierArticles;

    case ContentSuggestionsSectionUnknown:
      return SectionIdentifierDefault;
  }
}

}  // namespace

@interface ContentSuggestionsCollectionUpdater ()<
    ContentSuggestionsArticleItemDelegate,
    ContentSuggestionsDataSink>

@property(nonatomic, weak) id<ContentSuggestionsDataSource> dataSource;
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, ContentSuggestionsSectionInformation*>*
        sectionInfoBySectionIdentifier;

// Reloads all the data from the data source, deleting all the current items.
- (void)reloadData;
// Adds a new section if needed and returns the section identifier.
- (NSInteger)addSectionIfNeeded:
    (ContentSuggestionsSectionInformation*)sectionInformation;
// Resets the models, removing the current CollectionViewItem and the
// SectionInfo.
- (void)resetModels;

@end

@implementation ContentSuggestionsCollectionUpdater

@synthesize collectionViewController = _collectionViewController;
@synthesize dataSource = _dataSource;
@synthesize sectionInfoBySectionIdentifier = _sectionInfoBySectionIdentifier;

- (instancetype)initWithDataSource:
    (id<ContentSuggestionsDataSource>)dataSource {
  self = [super init];
  if (self) {
    _dataSource = dataSource;
    _dataSource.dataSink = self;
  }
  return self;
}

#pragma mark - Properties

- (void)setCollectionViewController:
    (ContentSuggestionsViewController*)collectionViewController {
  _collectionViewController = collectionViewController;

  [self reloadData];
}

#pragma mark - ContentSuggestionsDataSink

- (void)dataAvailable {
  [self reloadData];
}

#pragma mark - Public methods

- (BOOL)shouldUseCustomStyleForSection:(NSInteger)section {
  NSNumber* identifier = @([self.collectionViewController.collectionViewModel
      sectionIdentifierForSection:section]);
  ContentSuggestionsSectionInformation* sectionInformation =
      self.sectionInfoBySectionIdentifier[identifier];
  return sectionInformation.layout == ContentSuggestionsSectionLayoutCustom;
}

- (ContentSuggestionType)contentSuggestionTypeForItem:
    (CollectionViewItem*)item {
  return ContentSuggestionTypeForItemType(item.type);
}

#pragma mark - ContentSuggestionsArticleItemDelegate

- (void)loadImageForArticleItem:(ContentSuggestionsArticleItem*)articleItem {
  NSInteger sectionIdentifier =
      SectionIdentifierForInfo(articleItem.suggestionIdentifier.sectionInfo);

  __weak ContentSuggestionsCollectionUpdater* weakSelf = self;
  __weak ContentSuggestionsArticleItem* weakArticle = articleItem;
  void (^imageFetchedCallback)(const gfx::Image&) = ^(const gfx::Image& image) {
    if (image.IsEmpty()) {
      return;
    }

    ContentSuggestionsCollectionUpdater* strongSelf = weakSelf;
    ContentSuggestionsArticleItem* strongArticle = weakArticle;
    if (!strongSelf || !strongArticle) {
      return;
    }

    strongArticle.image = image.CopyUIImage();
    [strongSelf.collectionViewController
        reconfigureCellsForItems:@[ strongArticle ]
         inSectionWithIdentifier:sectionIdentifier];
  };

  [self.dataSource.imageFetcher
      fetchImageForSuggestion:articleItem.suggestionIdentifier
                     callback:imageFetchedCallback];
}

#pragma mark - Private methods

- (void)reloadData {
  [self resetModels];
  CollectionViewModel* model =
      self.collectionViewController.collectionViewModel;

  NSArray<ContentSuggestion*>* suggestions = [self.dataSource allSuggestions];

  for (ContentSuggestion* suggestion in suggestions) {
    NSInteger sectionIdentifier =
        [self addSectionIfNeeded:suggestion.suggestionIdentifier.sectionInfo];
    ContentSuggestionsArticleItem* articleItem =
        [[ContentSuggestionsArticleItem alloc]
            initWithType:ItemTypeForContentSuggestionType(suggestion.type)
                   title:suggestion.title
                subtitle:suggestion.text
                delegate:self
                     url:suggestion.url];

    articleItem.publisher = suggestion.publisher;
    articleItem.publishDate = suggestion.publishDate;

    articleItem.suggestionIdentifier = suggestion.suggestionIdentifier;

    [model addItem:articleItem toSectionWithIdentifier:sectionIdentifier];
  }

  if ([self.collectionViewController isViewLoaded]) {
    [self.collectionViewController.collectionView reloadData];
  }
}

- (NSInteger)addSectionIfNeeded:
    (ContentSuggestionsSectionInformation*)sectionInformation {
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInformation);

  CollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  if (![model hasSectionForSectionIdentifier:sectionIdentifier]) {
    [model addSectionWithIdentifier:sectionIdentifier];
    self.sectionInfoBySectionIdentifier[@(sectionIdentifier)] =
        sectionInformation;
    [self.sectionInfoBySectionIdentifier setObject:sectionInformation
                                            forKey:@(sectionIdentifier)];
  }
  return sectionIdentifier;
}

- (void)resetModels {
  [self.collectionViewController loadModel];
  self.sectionInfoBySectionIdentifier = [[NSMutableDictionary alloc] init];
}

@end
