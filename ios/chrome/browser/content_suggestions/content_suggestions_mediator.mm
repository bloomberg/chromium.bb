// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"

#include "base/mac/bind_objc_block.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/content_suggestion.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_service_bridge_observer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestion.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_fetcher.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// TODO(crbug.com/701275): Once base::BindBlock supports the move semantics,
// remove this wrapper.
// Wraps a callback taking a const ref to a callback taking an object.
void BindWrapper(
    base::Callback<void(ntp_snippets::Status status_code,
                        const std::vector<ntp_snippets::ContentSuggestion>&
                            suggestions)> callback,
    ntp_snippets::Status status_code,
    std::vector<ntp_snippets::ContentSuggestion> suggestions) {
  if (callback) {
    callback.Run(status_code, suggestions);
  }
}

// Returns the Type for this |category|.
ContentSuggestionType TypeForCategory(ntp_snippets::Category category) {
  // For now, only Article is a relevant type.
  return ContentSuggestionTypeArticle;
}

// Returns the section ID for this |category|.
ContentSuggestionsSectionID SectionIDForCategory(
    ntp_snippets::Category category) {
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::BOOKMARKS))
    return ContentSuggestionsSectionBookmarks;
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES))
    return ContentSuggestionsSectionArticles;

  return ContentSuggestionsSectionUnknown;
}

// Returns the section layout corresponding to the category |layout|.
ContentSuggestionsSectionLayout SectionLayoutForLayout(
    ntp_snippets::ContentSuggestionsCardLayout layout) {
  // For now, only cards are relevant.
  return ContentSuggestionsSectionLayoutCard;
}

// Converts a ntp_snippets::ContentSuggestion to an Objective-C
// ContentSuggestion.
ContentSuggestion* ConvertContentSuggestion(
    const ntp_snippets::ContentSuggestion& contentSuggestion) {
  ContentSuggestion* suggestion = [[ContentSuggestion alloc] init];

  suggestion.title = base::SysUTF16ToNSString(contentSuggestion.title());
  suggestion.text = base::SysUTF16ToNSString(contentSuggestion.snippet_text());
  suggestion.url = contentSuggestion.url();

  suggestion.publisher =
      base::SysUTF16ToNSString(contentSuggestion.publisher_name());
  suggestion.publishDate = contentSuggestion.publish_date();

  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.IDInSection =
      contentSuggestion.id().id_within_category();

  return suggestion;
}

// Returns a SectionInformation for a |category|, filled with the
// |categoryInfo|.
ContentSuggestionsSectionInformation* SectionInformationFromCategoryInfo(
    const base::Optional<ntp_snippets::CategoryInfo>& categoryInfo,
    const ntp_snippets::Category& category) {
  ContentSuggestionsSectionInformation* sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:SectionIDForCategory(category)];
  if (categoryInfo) {
    sectionInfo.layout = SectionLayoutForLayout(categoryInfo->card_layout());
    if (categoryInfo->show_if_empty()) {
      // TODO(crbug.com/686728): Creates an item to display information when the
      // section is empty.
    }
    if (categoryInfo->additional_action() !=
        ntp_snippets::ContentSuggestionsAdditionalAction::NONE) {
      sectionInfo.footerTitle =
          l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE);
    }
    sectionInfo.title = base::SysUTF16ToNSString(categoryInfo->title());
  }
  return sectionInfo;
}

// Returns a ntp_snippets::ID based on a Objective-C Category and the ID in the
// category.
ntp_snippets::ContentSuggestion::ID SuggestionIDForSectionID(
    ContentSuggestionsCategoryWrapper* category,
    const std::string& id_in_category) {
  return ntp_snippets::ContentSuggestion::ID(category.category, id_in_category);
}

}  // namespace

@interface ContentSuggestionsMediator ()<ContentSuggestionsImageFetcher,
                                         ContentSuggestionsServiceObserver> {
  // Bridge for this class to become an observer of a ContentSuggestionsService.
  std::unique_ptr<ContentSuggestionsServiceBridge> _suggestionBridge;
}

@property(nonatomic, assign)
    ntp_snippets::ContentSuggestionsService* contentService;
@property(nonatomic, strong, nonnull)
    NSMutableDictionary<ContentSuggestionsCategoryWrapper*,
                        ContentSuggestionsSectionInformation*>*
        sectionInformationByCategory;

// Converts the |suggestions| from |category| to ContentSuggestion and adds them
// to the |contentArray|  if the category is available.
- (void)addSuggestions:
            (const std::vector<ntp_snippets::ContentSuggestion>&)suggestions
          fromCategory:(ntp_snippets::Category&)category
               toArray:(NSMutableArray<ContentSuggestion*>*)contentArray;

// Adds the section information for |category| in
// self.sectionInformationByCategory.
- (void)addSectionInformationForCategory:(ntp_snippets::Category)category;

// Returns a CategoryWrapper acting as a key for this section info.
- (ContentSuggestionsCategoryWrapper*)categoryWrapperForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo;

@end

@implementation ContentSuggestionsMediator

@synthesize contentService = _contentService;
@synthesize dataSink = _dataSink;
@synthesize sectionInformationByCategory = _sectionInformationByCategory;

#pragma mark - Public

- (instancetype)initWithContentService:
    (ntp_snippets::ContentSuggestionsService*)contentService {
  self = [super init];
  if (self) {
    _suggestionBridge =
        base::MakeUnique<ContentSuggestionsServiceBridge>(self, contentService);
    _contentService = contentService;
    _sectionInformationByCategory = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void)dismissSuggestion:(ContentSuggestionIdentifier*)suggestionIdentifier {
  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [self categoryWrapperForSectionInfo:suggestionIdentifier.sectionInfo];
  ntp_snippets::ContentSuggestion::ID suggestion_id =
      ntp_snippets::ContentSuggestion::ID([categoryWrapper category],
                                          suggestionIdentifier.IDInSection);

  self.contentService->DismissSuggestion(suggestion_id);
}

#pragma mark - ContentSuggestionsDataSource

- (NSArray<ContentSuggestion*>*)allSuggestions {
  std::vector<ntp_snippets::Category> categories =
      self.contentService->GetCategories();
  NSMutableArray<ContentSuggestion*>* dataHolders = [NSMutableArray array];
  for (auto& category : categories) {
    const std::vector<ntp_snippets::ContentSuggestion>& suggestions =
        self.contentService->GetSuggestionsForCategory(category);
    [self addSuggestions:suggestions fromCategory:category toArray:dataHolders];
  }
  return dataHolders;
}

- (NSArray<ContentSuggestion*>*)suggestionsForSection:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  ntp_snippets::Category category =
      [[self categoryWrapperForSectionInfo:sectionInfo] category];

  NSMutableArray* convertedSuggestions = [NSMutableArray array];
  const std::vector<ntp_snippets::ContentSuggestion>& suggestions =
      self.contentService->GetSuggestionsForCategory(category);
  [self addSuggestions:suggestions
          fromCategory:category
               toArray:convertedSuggestions];
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
  std::set<std::string> known_suggestion_ids;
  for (ContentSuggestionIdentifier* identifier in knownSuggestions) {
    if (identifier.sectionInfo != sectionInfo)
      continue;
    known_suggestion_ids.insert(identifier.IDInSection);
  }

  ContentSuggestionsCategoryWrapper* wrapper =
      [self categoryWrapperForSectionInfo:sectionInfo];

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
                       callback:(void (^)(const gfx::Image&))callback {
  self.contentService->FetchSuggestionImage(
      SuggestionIDForSectionID(
          [self categoryWrapperForSectionInfo:suggestionIdentifier.sectionInfo],
          suggestionIdentifier.IDInSection),
      base::BindBlockArc(callback));
}

#pragma mark - Private

- (void)addSuggestions:
            (const std::vector<ntp_snippets::ContentSuggestion>&)suggestions
          fromCategory:(ntp_snippets::Category&)category
               toArray:(NSMutableArray<ContentSuggestion*>*)contentArray {
  if (self.contentService->GetCategoryStatus(category) !=
      ntp_snippets::CategoryStatus::AVAILABLE) {
    return;
  }

  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [ContentSuggestionsCategoryWrapper wrapperWithCategory:category];
  if (!self.sectionInformationByCategory[categoryWrapper]) {
    [self addSectionInformationForCategory:category];
  }

  for (auto& contentSuggestion : suggestions) {
    ContentSuggestion* suggestion = ConvertContentSuggestion(contentSuggestion);

    suggestion.type = TypeForCategory(category);

    suggestion.suggestionIdentifier.sectionInfo =
        self.sectionInformationByCategory[categoryWrapper];

    [contentArray addObject:suggestion];
  }
}

- (void)addSectionInformationForCategory:(ntp_snippets::Category)category {
  base::Optional<ntp_snippets::CategoryInfo> categoryInfo =
      self.contentService->GetCategoryInfo(category);

  ContentSuggestionsSectionInformation* sectionInfo =
      SectionInformationFromCategoryInfo(categoryInfo, category);

  self.sectionInformationByCategory[[ContentSuggestionsCategoryWrapper
      wrapperWithCategory:category]] = sectionInfo;
}

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
  if (statusCode.IsSuccess() && !suggestions.empty() && callback) {
    NSMutableArray<ContentSuggestion*>* contentSuggestions =
        [NSMutableArray array];
    ntp_snippets::Category category = suggestions[0].id().category();
    [self addSuggestions:suggestions
            fromCategory:category
                 toArray:contentSuggestions];
    callback(contentSuggestions);
  }
}

@end
