// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/mediator_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_tiles/metrics.h"
#include "components/rappor/rappor_service_impl.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

ContentSuggestionsSectionID SectionIDForCategory(
    ntp_snippets::Category category) {
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES))
    return ContentSuggestionsSectionArticles;
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::READING_LIST))
    return ContentSuggestionsSectionReadingList;

  return ContentSuggestionsSectionUnknown;
}

CollectionViewItem<SuggestedContent>* ConvertSuggestion(
    const ntp_snippets::ContentSuggestion& contentSuggestion,
    ContentSuggestionsSectionInformation* sectionInfo,
    ntp_snippets::Category category) {
  ContentSuggestionsItem* suggestion = [[ContentSuggestionsItem alloc]
      initWithType:0
             title:base::SysUTF16ToNSString(contentSuggestion.title())
          subtitle:base::SysUTF16ToNSString(contentSuggestion.snippet_text())
               url:contentSuggestion.url()];

  suggestion.publisher =
      base::SysUTF16ToNSString(contentSuggestion.publisher_name());
  suggestion.publishDate = contentSuggestion.publish_date();

  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.IDInSection =
      contentSuggestion.id().id_within_category();
  suggestion.suggestionIdentifier.sectionInfo = sectionInfo;

  if (category.IsKnownCategory(ntp_snippets::KnownCategories::READING_LIST)) {
    suggestion.availableOffline =
        contentSuggestion.reading_list_suggestion_extra()->distilled;
  }
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES)) {
    suggestion.hasImage = YES;
  }

  return suggestion;
}

ContentSuggestionsSectionInformation* SectionInformationFromCategoryInfo(
    const base::Optional<ntp_snippets::CategoryInfo>& categoryInfo,
    const ntp_snippets::Category& category) {
  ContentSuggestionsSectionInformation* sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:SectionIDForCategory(category)];
  if (categoryInfo) {
    sectionInfo.layout = ContentSuggestionsSectionLayoutCard;
    sectionInfo.showIfEmpty = categoryInfo->show_if_empty();
    sectionInfo.emptyText =
        base::SysUTF16ToNSString(categoryInfo->no_suggestions_message());
    if (categoryInfo->additional_action() !=
        ntp_snippets::ContentSuggestionsAdditionalAction::NONE) {
      sectionInfo.footerTitle =
          l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE);
    }
    sectionInfo.title = base::SysUTF16ToNSString(categoryInfo->title());
  }
  return sectionInfo;
}

ntp_snippets::ContentSuggestion::ID SuggestionIDForSectionID(
    ContentSuggestionsCategoryWrapper* category,
    const std::string& id_in_category) {
  return ntp_snippets::ContentSuggestion::ID(category.category, id_in_category);
}

ContentSuggestionsSectionInformation* MostVisitedSectionInformation() {
  ContentSuggestionsSectionInformation* sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:ContentSuggestionsSectionMostVisited];
  sectionInfo.title = nil;
  sectionInfo.footerTitle = nil;
  sectionInfo.showIfEmpty = NO;
  sectionInfo.layout = ContentSuggestionsSectionLayoutCustom;

  return sectionInfo;
}

void RecordPageImpression(const ntp_tiles::NTPTilesVector& mostVisited) {
  std::vector<ntp_tiles::metrics::TileImpression> tiles;
  for (const ntp_tiles::NTPTile& ntpTile : mostVisited) {
    tiles.emplace_back(ntpTile.source, ntp_tiles::UNKNOWN_TILE_TYPE,
                       ntpTile.url);
  }
  ntp_tiles::metrics::RecordPageImpression(
      tiles, GetApplicationContext()->GetRapporServiceImpl());
}

ContentSuggestionsMostVisitedItem* ConvertNTPTile(
    const ntp_tiles::NTPTile& tile,
    ContentSuggestionsSectionInformation* sectionInfo) {
  ContentSuggestionsMostVisitedItem* suggestion =
      [[ContentSuggestionsMostVisitedItem alloc] initWithType:0];

  suggestion.title = base::SysUTF16ToNSString(tile.title);
  suggestion.URL = tile.url;
  suggestion.source = tile.source;

  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.IDInSection = tile.url.spec();
  suggestion.suggestionIdentifier.sectionInfo = sectionInfo;

  return suggestion;
}
