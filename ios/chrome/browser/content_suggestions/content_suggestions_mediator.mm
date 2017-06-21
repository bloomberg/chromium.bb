// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"

#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_header_provider.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_service_bridge_observer.h"
#import "ios/chrome/browser/content_suggestions/mediator_util.h"
#include "ios/chrome/browser/ntp_tiles/most_visited_sites_observer_bridge.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_fetcher.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;

// Size of the favicon returned by the provider.
const CGFloat kDefaultFaviconSize = 48;
// Maximum number of most visited tiles fetched.
const NSInteger kMaxNumMostVisitedTiles = 8;

}  // namespace

@interface ContentSuggestionsMediator ()<ContentSuggestionsImageFetcher,
                                         ContentSuggestionsServiceObserver,
                                         MostVisitedSitesObserving> {
  // Bridge for this class to become an observer of a ContentSuggestionsService.
  std::unique_ptr<ContentSuggestionsServiceBridge> _suggestionBridge;
  std::unique_ptr<ntp_tiles::MostVisitedSites> _mostVisitedSites;
  std::unique_ptr<ntp_tiles::MostVisitedSitesObserverBridge> _mostVisitedBridge;
}

// Most visited items from the MostVisitedSites service currently displayed.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedItem*>* mostVisitedItems;
// Most visited items from the MostVisitedSites service (copied upon receiving
// the callback). Those items are up to date with the model.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedItem*>* freshMostVisitedItems;
// Section Info for the logo and omnibox section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* logoSectionInfo;
// Section Info for the Most Visited section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* mostVisitedSectionInfo;
// Whether the page impression has been recorded.
@property(nonatomic, assign) BOOL recordedPageImpression;
// The ContentSuggestionsService, serving suggestions.
@property(nonatomic, assign)
    ntp_snippets::ContentSuggestionsService* contentService;
// Map the section information created to the relevant category.
@property(nonatomic, strong, nonnull)
    NSMutableDictionary<ContentSuggestionsCategoryWrapper*,
                        ContentSuggestionsSectionInformation*>*
        sectionInformationByCategory;
// FaviconAttributesProvider to fetch the favicon for the suggestions.
@property(nonatomic, nullable, strong)
    FaviconAttributesProvider* attributesProvider;

@end

@implementation ContentSuggestionsMediator

@synthesize mostVisitedItems = _mostVisitedItems;
@synthesize freshMostVisitedItems = _freshMostVisitedItems;
@synthesize logoSectionInfo = _logoSectionInfo;
@synthesize mostVisitedSectionInfo = _mostVisitedSectionInfo;
@synthesize recordedPageImpression = _recordedPageImpression;
@synthesize contentService = _contentService;
@synthesize dataSink = _dataSink;
@synthesize sectionInformationByCategory = _sectionInformationByCategory;
@synthesize attributesProvider = _attributesProvider;
@synthesize commandHandler = _commandHandler;
@synthesize headerProvider = _headerProvider;

#pragma mark - Public

- (instancetype)
initWithContentService:(ntp_snippets::ContentSuggestionsService*)contentService
      largeIconService:(favicon::LargeIconService*)largeIconService
       mostVisitedSite:
           (std::unique_ptr<ntp_tiles::MostVisitedSites>)mostVisitedSites {
  self = [super init];
  if (self) {
    _suggestionBridge =
        base::MakeUnique<ContentSuggestionsServiceBridge>(self, contentService);
    _contentService = contentService;
    _sectionInformationByCategory = [[NSMutableDictionary alloc] init];
    _attributesProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kDefaultFaviconSize
             minFaviconSize:1
           largeIconService:largeIconService];

    _mostVisitedSectionInfo = MostVisitedSectionInformation();
    _logoSectionInfo = LogoSectionInformation();
    _mostVisitedSites = std::move(mostVisitedSites);
    _mostVisitedBridge =
        base::MakeUnique<ntp_tiles::MostVisitedSitesObserverBridge>(self);
    _mostVisitedSites->SetMostVisitedURLsObserver(_mostVisitedBridge.get(),
                                                  kMaxNumMostVisitedTiles);
  }
  return self;
}

- (void)blacklistMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlacklistedUrl(URL, true);
  [self useFreshMostVisited];
}

- (void)whitelistMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlacklistedUrl(URL, false);
  [self useFreshMostVisited];
}

#pragma mark - ContentSuggestionsDataSource

- (NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo {
  NSMutableArray<ContentSuggestionsSectionInformation*>* sectionsInfo =
      [NSMutableArray array];

  [sectionsInfo addObject:self.logoSectionInfo];

  if (self.mostVisitedItems.count > 0) {
    [sectionsInfo addObject:self.mostVisitedSectionInfo];
  }

  std::vector<ntp_snippets::Category> categories =
      self.contentService->GetCategories();

  for (auto& category : categories) {
    ContentSuggestionsCategoryWrapper* categoryWrapper =
        [ContentSuggestionsCategoryWrapper wrapperWithCategory:category];
    if (!self.sectionInformationByCategory[categoryWrapper]) {
      [self addSectionInformationForCategory:category];
    }
    [sectionsInfo addObject:self.sectionInformationByCategory[categoryWrapper]];
  }

  return sectionsInfo;
}

- (NSArray<CSCollectionViewItem*>*)itemsForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  NSMutableArray<CSCollectionViewItem*>* convertedSuggestions =
      [NSMutableArray array];

  if (sectionInfo == self.logoSectionInfo) {
    // TODO(crbug.com/732416): Add promo.
  } else if (sectionInfo == self.mostVisitedSectionInfo) {
    [convertedSuggestions addObjectsFromArray:self.mostVisitedItems];
  } else {
    ntp_snippets::Category category =
        [[self categoryWrapperForSectionInfo:sectionInfo] category];

    const std::vector<ntp_snippets::ContentSuggestion>& suggestions =
        self.contentService->GetSuggestionsForCategory(category);
    [self addSuggestions:suggestions
            fromCategory:category
             toItemArray:convertedSuggestions];
  }

  return convertedSuggestions;
}

- (id<ContentSuggestionsImageFetcher>)imageFetcher {
  return self;
}

- (void)fetchMoreSuggestionsKnowing:
            (NSArray<ContentSuggestionIdentifier*>*)knownSuggestions
                    fromSectionInfo:
                        (ContentSuggestionsSectionInformation*)sectionInfo
                           callback:(MoreSuggestionsFetched)callback {
  if (![self isRelatedToContentSuggestionsService:sectionInfo]) {
    callback(nil);
    return;
  }

  ContentSuggestionsCategoryWrapper* wrapper =
      [self categoryWrapperForSectionInfo:sectionInfo];

  base::Optional<ntp_snippets::CategoryInfo> categoryInfo =
      self.contentService->GetCategoryInfo([wrapper category]);

  if (!categoryInfo) {
    callback(nil);
    return;
  }

  switch (categoryInfo->additional_action()) {
    case ntp_snippets::ContentSuggestionsAdditionalAction::NONE:
      callback(nil);
      return;

    case ntp_snippets::ContentSuggestionsAdditionalAction::VIEW_ALL:
      callback(nil);
      if ([wrapper category].IsKnownCategory(
              ntp_snippets::KnownCategories::READING_LIST)) {
        [self.commandHandler openReadingList];
      }
      break;

    case ntp_snippets::ContentSuggestionsAdditionalAction::FETCH: {
      std::set<std::string> known_suggestion_ids;
      for (ContentSuggestionIdentifier* identifier in knownSuggestions) {
        if (identifier.sectionInfo != sectionInfo)
          continue;
        known_suggestion_ids.insert(identifier.IDInSection);
      }

      __weak ContentSuggestionsMediator* weakSelf = self;
      ntp_snippets::FetchDoneCallback serviceCallback = base::Bind(
          &BindWrapper,
          base::BindBlockArc(^void(
              ntp_snippets::Status status,
              const std::vector<ntp_snippets::ContentSuggestion>& suggestions) {
            [weakSelf didFetchMoreSuggestions:suggestions
                               withStatusCode:status
                                     callback:callback];
          }));

      self.contentService->Fetch([wrapper category], known_suggestion_ids,
                                 serviceCallback);

      break;
    }
  }
}

- (void)fetchFaviconAttributesForItem:(CSCollectionViewItem*)item
                           completion:(void (^)(FaviconAttributes*))completion {
  ContentSuggestionsSectionInformation* sectionInfo =
      item.suggestionIdentifier.sectionInfo;
  GURL url;
  if ([self isRelatedToContentSuggestionsService:sectionInfo]) {
    ContentSuggestionsItem* suggestionItem =
        base::mac::ObjCCast<ContentSuggestionsItem>(item);
    url = suggestionItem.URL;
  } else if (sectionInfo == self.mostVisitedSectionInfo) {
    ContentSuggestionsMostVisitedItem* mostVisited =
        base::mac::ObjCCast<ContentSuggestionsMostVisitedItem>(item);
    url = mostVisited.URL;
  }
  [self.attributesProvider fetchFaviconAttributesForURL:url
                                             completion:completion];
}

- (void)fetchFaviconImageForItem:(CSCollectionViewItem*)item
                      completion:(void (^)(UIImage*))completion {
  ContentSuggestionsSectionInformation* sectionInfo =
      item.suggestionIdentifier.sectionInfo;
  if (![self isRelatedToContentSuggestionsService:sectionInfo]) {
    return;
  }
  ContentSuggestionsItem* suggestionItem =
      base::mac::ObjCCast<ContentSuggestionsItem>(item);
  ntp_snippets::Category category =
      [[self categoryWrapperForSectionInfo:sectionInfo] category];
  if (!category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES)) {
    // TODO(crbug.com/721266): remove this guard once the choice to download the
    // favicon from the google server is done in the provider.
    return;
  }
  void (^imageCallback)(const gfx::Image&) = ^(const gfx::Image& image) {
    if (!image.IsEmpty()) {
      completion([image.ToUIImage() copy]);
    }
  };

  ntp_snippets::ContentSuggestion::ID identifier =
      ntp_snippets::ContentSuggestion::ID(
          category, suggestionItem.suggestionIdentifier.IDInSection);
  self.contentService->FetchSuggestionFavicon(
      identifier, /* minimum_size_in_pixel = */ 1, kDefaultFaviconSize,
      base::BindBlockArc(imageCallback));
}

- (void)dismissSuggestion:(ContentSuggestionIdentifier*)suggestionIdentifier {
  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [self categoryWrapperForSectionInfo:suggestionIdentifier.sectionInfo];
  ntp_snippets::ContentSuggestion::ID suggestion_id =
      ntp_snippets::ContentSuggestion::ID([categoryWrapper category],
                                          suggestionIdentifier.IDInSection);

  self.contentService->DismissSuggestion(suggestion_id);
}

- (UIView*)headerView {
  return [self.headerProvider header];
}

#pragma mark - ContentSuggestionsServiceObserver

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
         newSuggestionsInCategory:(ntp_snippets::Category)category {
  ContentSuggestionsCategoryWrapper* wrapper =
      [ContentSuggestionsCategoryWrapper wrapperWithCategory:category];
  if (!self.sectionInformationByCategory[wrapper]) {
    [self addSectionInformationForCategory:category];
  }
  [self.dataSink
      dataAvailableForSection:self.sectionInformationByCategory[wrapper]];
}

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
                         category:(ntp_snippets::Category)category
                  statusChangedTo:(ntp_snippets::CategoryStatus)status {
  if (!ntp_snippets::IsCategoryStatusInitOrAvailable(status)) {
    // Remove the category from the UI if it is not available.
    ContentSuggestionsCategoryWrapper* wrapper =
        [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category];
    ContentSuggestionsSectionInformation* sectionInfo =
        self.sectionInformationByCategory[wrapper];

    [self.dataSink clearSection:sectionInfo];
    [self.sectionInformationByCategory removeObjectForKey:wrapper];
  }
}

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
            suggestionInvalidated:
                (const ntp_snippets::ContentSuggestion::ID&)suggestion_id {
  ContentSuggestionsCategoryWrapper* wrapper =
      [[ContentSuggestionsCategoryWrapper alloc]
          initWithCategory:suggestion_id.category()];
  ContentSuggestionIdentifier* suggestionIdentifier =
      [[ContentSuggestionIdentifier alloc] init];
  suggestionIdentifier.IDInSection = suggestion_id.id_within_category();
  suggestionIdentifier.sectionInfo = self.sectionInformationByCategory[wrapper];

  [self.dataSink clearSuggestion:suggestionIdentifier];
}

- (void)contentSuggestionsServiceFullRefreshRequired:
    (ntp_snippets::ContentSuggestionsService*)suggestionsService {
  [self.dataSink reloadAllData];
}

- (void)contentSuggestionsServiceShutdown:
    (ntp_snippets::ContentSuggestionsService*)suggestionsService {
  // Update dataSink.
}

#pragma mark - ContentSuggestionsImageFetcher

- (void)fetchImageForSuggestion:
            (ContentSuggestionIdentifier*)suggestionIdentifier
                       callback:(void (^)(UIImage*))callback {
  self.contentService->FetchSuggestionImage(
      SuggestionIDForSectionID(
          [self categoryWrapperForSectionInfo:suggestionIdentifier.sectionInfo],
          suggestionIdentifier.IDInSection),
      base::BindBlockArc(^(const gfx::Image& image) {
        if (image.IsEmpty() || !callback) {
          return;
        }

        callback([image.ToUIImage() copy]);
      }));
}

#pragma mark - MostVisitedSitesObserving

- (void)onMostVisitedURLsAvailable:
    (const ntp_tiles::NTPTilesVector&)mostVisited {
  self.freshMostVisitedItems = [NSMutableArray array];
  for (const ntp_tiles::NTPTile& tile : mostVisited) {
    [self.freshMostVisitedItems
        addObject:ConvertNTPTile(tile, self.mostVisitedSectionInfo)];
  }

  if ([self.mostVisitedItems count] > 0) {
    // If some content is already displayed to the user, do not update without a
    // user action.
    return;
  }

  [self useFreshMostVisited];

  if (mostVisited.size() && !self.recordedPageImpression) {
    self.recordedPageImpression = YES;
    RecordPageImpression(mostVisited);
  }
}

- (void)onIconMadeAvailable:(const GURL&)siteURL {
  for (ContentSuggestionsMostVisitedItem* item in self.mostVisitedItems) {
    if (item.URL == siteURL) {
      [self.dataSink faviconAvailableForItem:item];
      return;
    }
  }
}

#pragma mark - Private

// Converts the |suggestions| from |category| to CSCollectionViewItem and adds
// them to the |contentArray| if the category is available.
- (void)addSuggestions:
            (const std::vector<ntp_snippets::ContentSuggestion>&)suggestions
          fromCategory:(ntp_snippets::Category&)category
           toItemArray:(NSMutableArray<CSCollectionViewItem*>*)itemArray {
  if (!ntp_snippets::IsCategoryStatusAvailable(
          self.contentService->GetCategoryStatus(category))) {
    return;
  }

  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [ContentSuggestionsCategoryWrapper wrapperWithCategory:category];
  if (!self.sectionInformationByCategory[categoryWrapper]) {
    [self addSectionInformationForCategory:category];
  }
  ContentSuggestionsSectionInformation* sectionInfo =
      self.sectionInformationByCategory[categoryWrapper];
  for (auto& contentSuggestion : suggestions) {
    CSCollectionViewItem* suggestion =
        ConvertSuggestion(contentSuggestion, sectionInfo, category);

    [itemArray addObject:suggestion];
  }
}

// Adds the section information for |category| in
// self.sectionInformationByCategory.
- (void)addSectionInformationForCategory:(ntp_snippets::Category)category {
  base::Optional<ntp_snippets::CategoryInfo> categoryInfo =
      self.contentService->GetCategoryInfo(category);

  ContentSuggestionsSectionInformation* sectionInfo =
      SectionInformationFromCategoryInfo(categoryInfo, category);

  self.sectionInformationByCategory[[ContentSuggestionsCategoryWrapper
      wrapperWithCategory:category]] = sectionInfo;
}

// Returns a CategoryWrapper acting as a key for this section info.
- (ContentSuggestionsCategoryWrapper*)categoryWrapperForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  return [[self.sectionInformationByCategory allKeysForObject:sectionInfo]
      firstObject];
}

// If the |statusCode| is a success and |suggestions| is not empty, runs the
// |callback| with the |suggestions| converted to Objective-C.
- (void)didFetchMoreSuggestions:
            (const std::vector<ntp_snippets::ContentSuggestion>&)suggestions
                 withStatusCode:(ntp_snippets::Status)statusCode
                       callback:(MoreSuggestionsFetched)callback {
  NSMutableArray<CSCollectionViewItem*>* contentSuggestions = nil;
  if (statusCode.IsSuccess() && !suggestions.empty() && callback) {
    contentSuggestions = [NSMutableArray array];
    ntp_snippets::Category category = suggestions[0].id().category();
    [self addSuggestions:suggestions
            fromCategory:category
             toItemArray:contentSuggestions];
  }
  callback(contentSuggestions);
}

// Returns whether the |sectionInfo| is associated with a category from the
// content suggestions service.
- (BOOL)isRelatedToContentSuggestionsService:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  return sectionInfo != self.mostVisitedSectionInfo &&
         sectionInfo != self.logoSectionInfo;
}

// Replaces the Most Visited items currently displayed by the most recent ones.
- (void)useFreshMostVisited {
  self.mostVisitedItems = self.freshMostVisitedItems;
  [self.dataSink reloadSection:self.mostVisitedSectionInfo];
}

@end
