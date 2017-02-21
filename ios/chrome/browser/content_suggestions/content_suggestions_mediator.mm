// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"

#include "base/mac/bind_objc_block.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestion.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_service_bridge_observer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestion.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_section_information.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

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
    sectionInfo.title = base::SysUTF16ToNSString(categoryInfo->title());
  }
  return sectionInfo;
}

}  // namespace

@interface ContentSuggestionsMediator ()<ContentSuggestionsServiceObserver> {
  // Bridge for this class to become an observer of a ContentSuggestionsService.
  std::unique_ptr<ContentSuggestionsServiceBridge> _suggestionBridge;
}

@property(nonatomic, assign)
    ntp_snippets::ContentSuggestionsService* contentService;
@property(nonatomic, strong, nonnull)
    NSMutableDictionary<ContentSuggestionsCategoryWrapper*,
                        ContentSuggestionsSectionInformation*>*
        sectionInformationByCategory;

// Converts the data in |category| to ContentSuggestion and adds them to the
// |contentArray|.
- (void)addContentInCategory:(ntp_snippets::Category&)category
                     toArray:(NSMutableArray<ContentSuggestion*>*)contentArray;

// Adds the section information for |category| in
// self.sectionInformationByCategory.
- (void)addSectionInformationForCategory:(ntp_snippets::Category)category;

@end

@implementation ContentSuggestionsMediator

@synthesize contentService = _contentService;
@synthesize dataSink = _dataSink;
@synthesize sectionInformationByCategory = _sectionInformationByCategory;

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

#pragma mark - ContentSuggestionsDataSource

- (NSArray<ContentSuggestion*>*)allSuggestions {
  std::vector<ntp_snippets::Category> categories =
      self.contentService->GetCategories();
  NSMutableArray<ContentSuggestion*>* dataHolders = [NSMutableArray array];
  for (auto& category : categories) {
    if (self.contentService->GetCategoryStatus(category) !=
        ntp_snippets::CategoryStatus::AVAILABLE) {
      continue;
    }
    if (!self.sectionInformationByCategory[
            [ContentSuggestionsCategoryWrapper wrapperWithCategory:category]]) {
      [self addSectionInformationForCategory:category];
    }
    [self addContentInCategory:category toArray:dataHolders];
  }
  return dataHolders;
}

#pragma mark - ContentSuggestionsServiceObserver

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
         newSuggestionsInCategory:(ntp_snippets::Category)category {
  [self.dataSink dataAvailable];
}

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
                         category:(ntp_snippets::Category)category
                  statusChangedTo:(ntp_snippets::CategoryStatus)status {
  // Update dataSink.
}

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
            SuggestionInvalidated:
                (const ntp_snippets::ContentSuggestion::ID&)suggestion_id {
  // Update dataSink.
}

- (void)contentSuggestionsServiceFullRefreshRequired:
    (ntp_snippets::ContentSuggestionsService*)suggestionsService {
  // Update dataSink.
}

- (void)contentSuggestionsServiceShutdown:
    (ntp_snippets::ContentSuggestionsService*)suggestionsService {
  // Update dataSink.
}

#pragma mark - Private

- (void)addContentInCategory:(ntp_snippets::Category&)category
                     toArray:(NSMutableArray<ContentSuggestion*>*)contentArray {
  const std::vector<ntp_snippets::ContentSuggestion>& suggestions =
      self.contentService->GetSuggestionsForCategory(category);
  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category];
  for (auto& contentSuggestion : suggestions) {
    ContentSuggestion* suggestion = ConvertContentSuggestion(contentSuggestion);
    suggestion.type = TypeForCategory(category);
    suggestion.section = self.sectionInformationByCategory[categoryWrapper];

    // TODO(crbug.com/686728): fetch the image.

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

@end
