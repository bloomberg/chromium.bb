// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item+collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_footer_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_header_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_text_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
using CSCollectionViewModel = CollectionViewModel<CSCollectionViewItem*>;

// Enum defining the ItemType of this ContentSuggestionsCollectionUpdater.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeArticle = kItemTypeEnumZero,
  ItemTypeFooter,
  ItemTypeHeader,
  ItemTypeEmpty,
  ItemTypeReadingList,
  ItemTypeMostVisited,
  ItemTypePromo,
  ItemTypeUnknown,
};

// Enum defining the SectionIdentifier of this
// ContentSuggestionsCollectionUpdater.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierArticles = kSectionIdentifierEnumZero,
  SectionIdentifierReadingList,
  SectionIdentifierMostVisited,
  SectionIdentifierLogo,
  SectionIdentifierDefault,
};

// Returns the ContentSuggestionType associated with an ItemType |type|.
ContentSuggestionType ContentSuggestionTypeForItemType(NSInteger type) {
  if (type == ItemTypeArticle)
    return ContentSuggestionTypeArticle;
  if (type == ItemTypeEmpty)
    return ContentSuggestionTypeEmpty;
  if (type == ItemTypeReadingList)
    return ContentSuggestionTypeReadingList;
  if (type == ItemTypeMostVisited)
    return ContentSuggestionTypeMostVisited;
  if (type == ItemTypePromo)
    return ContentSuggestionTypePromo;
  // Add new type here

  // Default type.
  return ContentSuggestionTypeEmpty;
}

// Returns the item type corresponding to the section |info|.
ItemType ItemTypeForInfo(ContentSuggestionsSectionInformation* info) {
  switch (info.sectionID) {
    case ContentSuggestionsSectionArticles:
      return ItemTypeArticle;
    case ContentSuggestionsSectionReadingList:
      return ItemTypeReadingList;
    case ContentSuggestionsSectionMostVisited:
      return ItemTypeMostVisited;
    case ContentSuggestionsSectionLogo:
      return ItemTypePromo;

    case ContentSuggestionsSectionUnknown:
      return ItemTypeUnknown;
  }
}

// Returns the section identifier corresponding to the section |info|.
SectionIdentifier SectionIdentifierForInfo(
    ContentSuggestionsSectionInformation* info) {
  switch (info.sectionID) {
    case ContentSuggestionsSectionArticles:
      return SectionIdentifierArticles;
    case ContentSuggestionsSectionReadingList:
      return SectionIdentifierReadingList;
    case ContentSuggestionsSectionMostVisited:
      return SectionIdentifierMostVisited;
    case ContentSuggestionsSectionLogo:
      return SectionIdentifierLogo;

    case ContentSuggestionsSectionUnknown:
      return SectionIdentifierDefault;
  }
}

const CGFloat kNumberOfMostVisitedLines = 2;

}  // namespace

@interface ContentSuggestionsCollectionUpdater ()<ContentSuggestionsDataSink>

@property(nonatomic, weak) id<ContentSuggestionsDataSource> dataSource;
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, ContentSuggestionsSectionInformation*>*
        sectionInfoBySectionIdentifier;
// Width of the collection. Upon size change, it reflects the new size.
@property(nonatomic, assign) CGFloat collectionWidth;
// Whether an item of type ItemTypePromo has already been added to the model.
@property(nonatomic, assign) BOOL promoAdded;

@end

@implementation ContentSuggestionsCollectionUpdater

@synthesize collectionViewController = _collectionViewController;
@synthesize dataSource = _dataSource;
@synthesize sectionInfoBySectionIdentifier = _sectionInfoBySectionIdentifier;
@synthesize collectionWidth = _collectionWidth;
@synthesize promoAdded = _promoAdded;

- (instancetype)initWithDataSource:
    (id<ContentSuggestionsDataSource>)dataSource {
  self = [super init];
  if (self) {
    _promoAdded = NO;
    _dataSource = dataSource;
    _dataSource.dataSink = self;
  }
  return self;
}

#pragma mark - Properties

- (void)setCollectionViewController:
    (ContentSuggestionsViewController*)collectionViewController {
  _collectionViewController = collectionViewController;
  self.collectionWidth =
      collectionViewController.collectionView.bounds.size.width;

  [self reloadAllData];
}

#pragma mark - ContentSuggestionsDataSink

- (void)dataAvailableForSection:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSArray<CSCollectionViewItem*>* items =
        [model itemsInSectionWithIdentifier:sectionIdentifier];
    if (items.count > 0 && items[0].type != ItemTypeEmpty) {
      // Do not dismiss the presented items.
      return;
    }
  }

  [self.collectionViewController
      addSuggestions:[self.dataSource itemsForSectionInfo:sectionInfo]
       toSectionInfo:sectionInfo];
}

- (void)clearSuggestion:(ContentSuggestionIdentifier*)suggestionIdentifier {
  SectionIdentifier sectionIdentifier =
      SectionIdentifierForInfo(suggestionIdentifier.sectionInfo);
  if (![self.collectionViewController.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    return;
  }

  NSArray<CSCollectionViewItem*>* itemsInSection =
      [self.collectionViewController.collectionViewModel
          itemsInSectionWithIdentifier:sectionIdentifier];

  CSCollectionViewItem* correspondingItem = nil;
  for (CSCollectionViewItem* item in itemsInSection) {
    if (item.suggestionIdentifier == suggestionIdentifier) {
      correspondingItem = item;
      break;
    }
  }

  if (!correspondingItem)
    return;

  NSIndexPath* indexPath = [self.collectionViewController.collectionViewModel
      indexPathForItem:correspondingItem];
  [self.collectionViewController dismissEntryAtIndexPath:indexPath];
}

- (void)reloadAllData {
  [self resetModels];

  // The data is reset, add the new data directly in the model then reload the
  // collection.
  NSArray<ContentSuggestionsSectionInformation*>* sectionInfos =
      [self.dataSource sectionsInfo];
  [self addSectionsForSectionInfoToModel:sectionInfos];
  for (ContentSuggestionsSectionInformation* sectionInfo in sectionInfos) {
    [self
        addSuggestionsToModel:[self.dataSource itemsForSectionInfo:sectionInfo]
              withSectionInfo:sectionInfo];
  }
  [self.collectionViewController.collectionView reloadData];
}

- (void)clearSection:(ContentSuggestionsSectionInformation*)sectionInfo {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);
  NSInteger section = [self.collectionViewController.collectionViewModel
      sectionIdentifierForSection:sectionIdentifier];

  [self.collectionViewController dismissSection:section];
}

- (void)reloadSection:(ContentSuggestionsSectionInformation*)sectionInfo {
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  if (![model hasSectionForSectionIdentifier:sectionIdentifier]) {
    [self.collectionViewController
        addSuggestions:[self.dataSource itemsForSectionInfo:sectionInfo]
         toSectionInfo:sectionInfo];
    return;
  }

  NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];

  NSMutableArray* oldItems = [NSMutableArray array];
  NSInteger numberOfItems = [model numberOfItemsInSection:section];
  for (NSInteger i = 0; i < numberOfItems; i++) {
    [oldItems addObject:[NSIndexPath indexPathForItem:i inSection:section]];
  }
  [self.collectionViewController
                   collectionView:self.collectionViewController.collectionView
      willDeleteItemsAtIndexPaths:oldItems];

  [self addSuggestionsToModel:[self.dataSource itemsForSectionInfo:sectionInfo]
              withSectionInfo:sectionInfo];

  [self.collectionViewController.collectionView
      reloadSections:[NSIndexSet indexSetWithIndex:section]];
}

- (void)itemHasChanged:(CollectionViewItem<SuggestedContent>*)item {
  if (![self.collectionViewController.collectionViewModel hasItem:item]) {
    return;
  }
  [self.collectionViewController reconfigureCellsForItems:@[ item ]];
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

- (NSIndexPath*)removeEmptySuggestionsForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  NSArray<CSCollectionViewItem*>* existingItems =
      [model itemsInSectionWithIdentifier:sectionIdentifier];
  if (existingItems.count == 1 && existingItems[0].type == ItemTypeEmpty) {
    [model removeItemWithType:ItemTypeEmpty
        fromSectionWithIdentifier:sectionIdentifier];
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    return [NSIndexPath indexPathForItem:0 inSection:section];
  }
  return nil;
}

- (NSArray<NSIndexPath*>*)
addSuggestionsToModel:(NSArray<CSCollectionViewItem*>*)suggestions
      withSectionInfo:(ContentSuggestionsSectionInformation*)sectionInfo {
  NSMutableArray<NSIndexPath*>* indexPaths = [NSMutableArray array];

  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  if (suggestions.count == 0) {
    if ([model hasSectionForSectionIdentifier:sectionIdentifier] &&
        [model numberOfItemsInSection:[model sectionForSectionIdentifier:
                                                 sectionIdentifier]] == 0) {
      NSIndexPath* emptyItemIndexPath =
          [self addEmptyItemForSection:
                    [model sectionForSectionIdentifier:sectionIdentifier]];
      if (emptyItemIndexPath) {
        [indexPaths addObject:emptyItemIndexPath];
      }
    }
    return indexPaths;
  }

  [suggestions enumerateObjectsUsingBlock:^(CSCollectionViewItem* item,
                                            NSUInteger index, BOOL* stop) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    if ([self isMostVisitedSection:section] &&
        [model numberOfItemsInSection:section] >=
            [self mostVisitedPlaceCount]) {
      return;
    }
    ItemType type = ItemTypeForInfo(sectionInfo);
    if (type == ItemTypePromo && !self.promoAdded) {
      self.promoAdded = YES;
      [self.collectionViewController.audience promoShown];
    }
    item.type = type;
    NSIndexPath* addedIndexPath =
        [self addItem:item toSectionWithIdentifier:sectionIdentifier];

    [indexPaths addObject:addedIndexPath];
  }];

  return indexPaths;
}

- (NSIndexSet*)addSectionsForSectionInfoToModel:
    (NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo {
  NSMutableIndexSet* addedSectionIdentifiers = [NSMutableIndexSet indexSet];
  NSArray<ContentSuggestionsSectionInformation*>* orderedSectionsInfo =
      [self.dataSource sectionsInfo];

  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  for (ContentSuggestionsSectionInformation* sectionInfo in sectionsInfo) {
    NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

    if ([model hasSectionForSectionIdentifier:sectionIdentifier] ||
        (!sectionInfo.showIfEmpty &&
         [self.dataSource itemsForSectionInfo:sectionInfo].count == 0)) {
      continue;
    }

    NSUInteger sectionIndex = 0;
    for (ContentSuggestionsSectionInformation* orderedSectionInfo in
             orderedSectionsInfo) {
      NSInteger orderedSectionIdentifier =
          SectionIdentifierForInfo(orderedSectionInfo);
      if (orderedSectionIdentifier == sectionIdentifier) {
        break;
      }
      if ([model hasSectionForSectionIdentifier:orderedSectionIdentifier]) {
        sectionIndex++;
      }
    }
    [model insertSectionWithIdentifier:sectionIdentifier atIndex:sectionIndex];

    self.sectionInfoBySectionIdentifier[@(sectionIdentifier)] = sectionInfo;
    [addedSectionIdentifiers addIndex:sectionIdentifier];

    if (sectionIdentifier == SectionIdentifierLogo) {
      [self addLogoHeaderIfNeeded];
    } else {
      [self addHeaderIfNeeded:sectionInfo];
    }
    [self addFooterIfNeeded:sectionInfo];
  }

  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
  [addedSectionIdentifiers enumerateIndexesUsingBlock:^(
                               NSUInteger sectionIdentifier,
                               BOOL* _Nonnull stop) {
    [indexSet addIndex:[model sectionForSectionIdentifier:sectionIdentifier]];
  }];
  return indexSet;
}

- (NSIndexPath*)addEmptyItemForSection:(NSInteger)section {
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  NSInteger sectionIdentifier = [model sectionIdentifierForSection:section];
  ContentSuggestionsSectionInformation* sectionInfo =
      self.sectionInfoBySectionIdentifier[@(sectionIdentifier)];

  CSCollectionViewItem* item = [self emptyItemForSectionInfo:sectionInfo];
  if (!item) {
    return nil;
  }
  return [self addItem:item toSectionWithIdentifier:sectionIdentifier];
}

- (BOOL)isMostVisitedSection:(NSInteger)section {
  return
      [self.collectionViewController.collectionViewModel
          sectionIdentifierForSection:section] == SectionIdentifierMostVisited;
}

- (BOOL)isHeaderSection:(NSInteger)section {
  return [self.collectionViewController.collectionViewModel
             sectionIdentifierForSection:section] == SectionIdentifierLogo;
}

- (void)updateMostVisitedForSize:(CGSize)size {
  self.collectionWidth = size.width;

  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  if (![model hasSectionForSectionIdentifier:SectionIdentifierMostVisited])
    return;

  NSInteger mostVisitedSection =
      [model sectionForSectionIdentifier:SectionIdentifierMostVisited];
  ContentSuggestionsSectionInformation* mostVisitedSectionInfo =
      self.sectionInfoBySectionIdentifier[@(SectionIdentifierMostVisited)];
  NSArray<CSCollectionViewItem*>* mostVisited =
      [self.dataSource itemsForSectionInfo:mostVisitedSectionInfo];
  NSInteger newCount = MIN([self mostVisitedPlaceCount],
                           static_cast<NSInteger>(mostVisited.count));
  NSInteger currentCount = [model numberOfItemsInSection:mostVisitedSection];

  if (currentCount == newCount)
    return;

  if (currentCount > newCount) {
    for (NSInteger i = newCount; i < currentCount; i++) {
      [self.collectionViewController.collectionViewModel
                 removeItemWithType:ItemTypeMostVisited
          fromSectionWithIdentifier:SectionIdentifierMostVisited
                            atIndex:newCount];
    }
  } else {
    for (NSInteger i = currentCount; i < newCount; i++) {
      CSCollectionViewItem* item = mostVisited[i];
      item.type = ItemTypeMostVisited;
      [self.collectionViewController.collectionViewModel
                          addItem:item
          toSectionWithIdentifier:SectionIdentifierMostVisited];
    }
  }
}

- (void)dismissItem:(CSCollectionViewItem*)item {
  [self.dataSource dismissSuggestion:item.suggestionIdentifier];
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

// Adds the header corresponding to |sectionInfo| to the section if there is
// none present and the section info contains a title.
- (void)addHeaderIfNeeded:(ContentSuggestionsSectionInformation*)sectionInfo {
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  if (![self.collectionViewController.collectionViewModel
          headerForSectionWithIdentifier:sectionIdentifier] &&
      sectionInfo.title) {
    CollectionViewTextItem* header =
        [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader];
    header.text = sectionInfo.title;
    header.textColor = [[MDCPalette greyPalette] tint500];
    [self.collectionViewController.collectionViewModel
                       setHeader:header
        forSectionWithIdentifier:sectionIdentifier];
  }
}

// Adds the header for the first section, containing the logo and the omnibox,
// if there is no header for the section.
- (void)addLogoHeaderIfNeeded {
  if (![self.collectionViewController.collectionViewModel
          headerForSectionWithIdentifier:SectionIdentifierLogo]) {
    ContentSuggestionsHeaderItem* header =
        [[ContentSuggestionsHeaderItem alloc] initWithType:ItemTypeHeader];
    header.view = [self.dataSource headerViewForWidth:self.collectionWidth];
    [self.collectionViewController.collectionViewModel
                       setHeader:header
        forSectionWithIdentifier:SectionIdentifierLogo];
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

  // TODO(crbug.com/721229): Start spinner.

  NSMutableArray<ContentSuggestionIdentifier*>* knownSuggestionIdentifiers =
      [NSMutableArray array];

  NSArray<CSCollectionViewItem*>* knownSuggestions =
      [self.collectionViewController.collectionViewModel
          itemsInSectionWithIdentifier:sectionIdentifier];
  for (CSCollectionViewItem* suggestion in knownSuggestions) {
    if (suggestion.type != ItemTypeEmpty) {
      [knownSuggestionIdentifiers addObject:suggestion.suggestionIdentifier];
    }
  }

  __weak ContentSuggestionsCollectionUpdater* weakSelf = self;
  [self.dataSource
      fetchMoreSuggestionsKnowing:knownSuggestionIdentifiers
                  fromSectionInfo:sectionInfo
                         callback:^(
                             NSArray<CSCollectionViewItem*>* suggestions) {
                           [weakSelf moreSuggestionsFetched:suggestions
                                              inSectionInfo:sectionInfo];
                         }];
}

// Adds the |suggestions| to the collection view. All the suggestions must have
// the same sectionInfo.
- (void)moreSuggestionsFetched:(NSArray<CSCollectionViewItem*>*)suggestions
                 inSectionInfo:
                     (ContentSuggestionsSectionInformation*)sectionInfo {
  if (suggestions) {
    [self.collectionViewController addSuggestions:suggestions
                                    toSectionInfo:sectionInfo];
  }
  // TODO(crbug.com/721229):Stop spinner.
}

// Returns a item to be displayed when the section identified by |sectionInfo|
// is empty.
// Returns nil if there is no empty item for this section info.
- (CSCollectionViewItem*)emptyItemForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  if (!sectionInfo.emptyText)
    return nil;
  ContentSuggestionsTextItem* item =
      [[ContentSuggestionsTextItem alloc] initWithType:ItemTypeEmpty];
  item.text = l10n_util::GetNSString(IDS_NTP_TITLE_NO_SUGGESTIONS);
  item.detailText = sectionInfo.emptyText;

  return item;
}

// Adds |item| to |sectionIdentifier| section of the model of the
// CollectionView. Returns the IndexPath of the newly added item.
- (NSIndexPath*)addItem:(CSCollectionViewItem*)item
    toSectionWithIdentifier:(NSInteger)sectionIdentifier {
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
  NSInteger itemNumber = [model numberOfItemsInSection:section];
  [model addItem:item toSectionWithIdentifier:sectionIdentifier];

  return [NSIndexPath indexPathForItem:itemNumber inSection:section];
}

// Returns the maximum number of Most Visited tiles to be displayed in the
// collection.
- (NSInteger)mostVisitedPlaceCount {
  return content_suggestions::numberOfTilesForWidth(self.collectionWidth) *
         kNumberOfMostVisitedLines;
}

@end
