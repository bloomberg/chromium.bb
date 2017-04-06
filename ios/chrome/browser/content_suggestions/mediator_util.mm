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

ContentSuggestionType TypeForCategory(ntp_snippets::Category category) {
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES))
    return ContentSuggestionTypeArticle;
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::READING_LIST))
    return ContentSuggestionTypeReadingList;

  return ContentSuggestionTypeEmpty;
}

ContentSuggestionsSectionID SectionIDForCategory(
    ntp_snippets::Category category) {
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES))
    return ContentSuggestionsSectionArticles;
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::READING_LIST))
    return ContentSuggestionsSectionReadingList;

  return ContentSuggestionsSectionUnknown;
}

ContentSuggestionsSectionLayout SectionLayoutForLayout(
    ntp_snippets::ContentSuggestionsCardLayout layout) {
  // For now, only cards are relevant.
  return ContentSuggestionsSectionLayoutCard;
}

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

ContentSuggestionsSectionInformation* SectionInformationFromCategoryInfo(
    const base::Optional<ntp_snippets::CategoryInfo>& categoryInfo,
    const ntp_snippets::Category& category) {
  ContentSuggestionsSectionInformation* sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:SectionIDForCategory(category)];
  if (categoryInfo) {
    sectionInfo.layout = SectionLayoutForLayout(categoryInfo->card_layout());
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

ContentSuggestion* EmptySuggestion() {
  ContentSuggestion* suggestion = [[ContentSuggestion alloc] init];
  suggestion.type = ContentSuggestionTypeEmpty;
  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];

  return suggestion;
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

ContentSuggestion* ConvertNTPTile(const ntp_tiles::NTPTile& tile) {
  ContentSuggestion* suggestion = [[ContentSuggestion alloc] init];

  suggestion.title = base::SysUTF16ToNSString(tile.title);
  suggestion.url = tile.url;
  suggestion.type = ContentSuggestionTypeMostVisited;

  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.IDInSection = tile.url.spec();

  return suggestion;
}
