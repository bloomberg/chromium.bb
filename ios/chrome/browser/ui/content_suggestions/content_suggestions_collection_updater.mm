// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_article_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_button_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_expandable_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_favicon_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_footer_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_stack_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestion.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_fetcher.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
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
  ItemTypeFooter,
  ItemTypeHeader,
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

  [self reloadAllData];
}

#pragma mark - ContentSuggestionsDataSink

- (void)dataAvailableForSection:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  CollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier] &&
      [model itemsInSectionWithIdentifier:sectionIdentifier].count > 0) {
    // Do not dismiss the presented items.
    return;
  }

  [self.collectionViewController
      addSuggestions:[self.dataSource suggestionsForSection:sectionInfo]];
}

- (void)clearSuggestion:(ContentSuggestionIdentifier*)suggestionIdentifier {
  SectionIdentifier sectionIdentifier =
      SectionIdentifierForInfo(suggestionIdentifier.sectionInfo);
  if (![self.collectionViewController.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    return;
  }

  NSArray<CollectionViewItem<ContentSuggestionIdentification>*>*
      itemsInSection = [self.collectionViewController.collectionViewModel
          itemsInSectionWithIdentifier:sectionIdentifier];

  CollectionViewItem<ContentSuggestionIdentification>* correspondingItem = nil;
  for (CollectionViewItem<ContentSuggestionIdentification>* item in
           itemsInSection) {
    if (item.suggestionIdentifier == suggestionIdentifier) {
      correspondingItem = item;
      break;
    }
  }

  if (!correspondingItem)
    return;

  NSIndexPath* indexPath = [self.collectionViewController.collectionViewModel
             indexPathForItem:correspondingItem
      inSectionWithIdentifier:sectionIdentifier];
  [self.collectionViewController dismissEntryAtIndexPath:indexPath];
}

- (void)reloadAllData {
  [self resetModels];

  // The data is reset, add the new data directly in the model then reload the
  // collection.
  NSArray<ContentSuggestion*>* suggestions = [self.dataSource allSuggestions];
  [self addSectionsForSuggestionsToModel:suggestions];
  [self addSuggestionsToModel:suggestions];
  [self.collectionViewController.collectionView reloadData];
}

- (void)clearSection:(ContentSuggestionsSectionInformation*)sectionInfo {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);
  NSInteger section = [self.collectionViewController.collectionViewModel
      sectionIdentifierForSection:sectionIdentifier];

  [self.collectionViewController dismissSection:section];
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

- (NSArray<NSIndexPath*>*)addSuggestionsToModel:
    (NSArray<ContentSuggestion*>*)suggestions {
  if (suggestions.count == 0) {
    return [NSArray array];
  }

  CollectionViewModel* model =
      self.collectionViewController.collectionViewModel;

  NSMutableArray<NSIndexPath*>* indexPaths = [NSMutableArray array];
  for (ContentSuggestion* suggestion in suggestions) {
    NSInteger sectionIdentifier =
        SectionIdentifierForInfo(suggestion.suggestionIdentifier.sectionInfo);

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

    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    NSInteger itemNumber = [model numberOfItemsInSection:section];
    [model addItem:articleItem toSectionWithIdentifier:sectionIdentifier];

    [indexPaths
        addObject:[NSIndexPath indexPathForItem:itemNumber inSection:section]];
  }

  return indexPaths;
}

- (NSIndexSet*)addSectionsForSuggestionsToModel:
    (NSArray<ContentSuggestion*>*)suggestions {
  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];

  CollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  for (ContentSuggestion* suggestion in suggestions) {
    ContentSuggestionsSectionInformation* sectionInfo =
        suggestion.suggestionIdentifier.sectionInfo;
    NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

    if (![model hasSectionForSectionIdentifier:sectionIdentifier]) {
      [model addSectionWithIdentifier:sectionIdentifier];
      self.sectionInfoBySectionIdentifier[@(sectionIdentifier)] = sectionInfo;
      [indexSet addIndex:[model sectionForSectionIdentifier:sectionIdentifier]];

      [self addHeader:suggestion.suggestionIdentifier.sectionInfo];
      [self addFooterIfNeeded:suggestion.suggestionIdentifier.sectionInfo];
    }
  }
  return indexSet;
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

// Adds a footer to the section identified by |sectionInfo| if there is none
// present and the section info contains a title for it.
- (void)addFooterIfNeeded:(ContentSuggestionsSectionInformation*)sectionInfo {
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  __weak ContentSuggestionsCollectionUpdater* weakSelf = self;
  if (sectionInfo.footerTitle &&
      ![self.collectionViewController.collectionViewModel
          footerForSectionWithIdentifier:sectionIdentifier]) {
    ContentSuggestionsFooterItem* footer = [[ContentSuggestionsFooterItem alloc]
        initWithType:ItemTypeFooter
               title:sectionInfo.footerTitle
               block:^{
                 [weakSelf runAdditionalActionForSection:sectionInfo];
               }];

    [self.collectionViewController.collectionViewModel
                       setFooter:footer
        forSectionWithIdentifier:sectionIdentifier];
  }
}

// Adds the header corresponding to |sectionInfo| to the section.
- (void)addHeader:(ContentSuggestionsSectionInformation*)sectionInfo {
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  if (![self.collectionViewController.collectionViewModel
          headerForSectionWithIdentifier:sectionIdentifier]) {
    CollectionViewTextItem* header =
        [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader];
    header.text = sectionInfo.title;
    [self.collectionViewController.collectionViewModel
                       setHeader:header
        forSectionWithIdentifier:sectionIdentifier];
  }
}

// Resets the models, removing the current CollectionViewItem and the
// SectionInfo.
- (void)resetModels {
  [self.collectionViewController loadModel];
  self.sectionInfoBySectionIdentifier = [[NSMutableDictionary alloc] init];
}

// Runs the additional action for the section identified by |sectionInfo|.
- (void)runAdditionalActionForSection:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  NSMutableArray<ContentSuggestionIdentifier*>* knownSuggestionIdentifiers =
      [NSMutableArray array];

  NSArray<CollectionViewItem<ContentSuggestionIdentification>*>*
      knownSuggestions = [self.collectionViewController.collectionViewModel
          itemsInSectionWithIdentifier:sectionIdentifier];
  for (CollectionViewItem<ContentSuggestionIdentification>* suggestion in
           knownSuggestions) {
    [knownSuggestionIdentifiers addObject:suggestion.suggestionIdentifier];
  }

  __weak ContentSuggestionsCollectionUpdater* weakSelf = self;
  [self.dataSource
      fetchMoreSuggestionsKnowing:knownSuggestionIdentifiers
                  fromSectionInfo:sectionInfo
                         callback:^(NSArray<ContentSuggestion*>* suggestions) {
                           [weakSelf moreSuggestionsFetched:suggestions];
                         }];
}

// Adds the |suggestions| to the collection view. All the suggestions must have
// the same sectionInfo.
- (void)moreSuggestionsFetched:(NSArray<ContentSuggestion*>*)suggestions {
  [self.collectionViewController addSuggestions:suggestions];
}

@end
