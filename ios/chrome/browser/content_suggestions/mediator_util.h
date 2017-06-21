// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_

#include "base/bind.h"
#include "base/optional.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/status.h"
#include "components/ntp_tiles/ntp_tile.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"

namespace ntp_snippets {
class Category;
}

@class CollectionViewItem;
@class ContentSuggestionsCategoryWrapper;
@class ContentSuggestionsMostVisitedItem;

// TODO(crbug.com/701275): Once base::BindBlock supports the move semantics,
// remove this wrapper.
// Wraps a callback taking a const ref to a callback taking an object.
void BindWrapper(
    base::Callback<void(ntp_snippets::Status status_code,
                        const std::vector<ntp_snippets::ContentSuggestion>&
                            suggestions)> callback,
    ntp_snippets::Status status_code,
    std::vector<ntp_snippets::ContentSuggestion> suggestions);

// Returns the section ID for this |category|.
ContentSuggestionsSectionID SectionIDForCategory(
    ntp_snippets::Category category);

// Converts a ntp_snippets::ContentSuggestion to a CollectionViewItem.
CollectionViewItem<SuggestedContent>* ConvertSuggestion(
    const ntp_snippets::ContentSuggestion& contentSuggestion,
    ContentSuggestionsSectionInformation* sectionInfo,
    ntp_snippets::Category category);

// Returns a SectionInformation for a |category|, filled with the
// |categoryInfo|.
ContentSuggestionsSectionInformation* SectionInformationFromCategoryInfo(
    const base::Optional<ntp_snippets::CategoryInfo>& categoryInfo,
    const ntp_snippets::Category& category);

// Returns a ntp_snippets::ID based on a Objective-C Category and the ID in the
// category.
ntp_snippets::ContentSuggestion::ID SuggestionIDForSectionID(
    ContentSuggestionsCategoryWrapper* category,
    const std::string& id_in_category);

// Creates and returns a SectionInfo for the section containing the logo and
// omnibox.
ContentSuggestionsSectionInformation* LogoSectionInformation();

// Creates and returns a SectionInfo for the Most Visited section.
ContentSuggestionsSectionInformation* MostVisitedSectionInformation();

// Records the page impression of the ntp tiles.
void RecordPageImpression(const std::vector<ntp_tiles::NTPTile>& mostVisited);

// Converts a ntp_tiles::NTPTile |tile| to a ContentSuggestionsMostVisitedItem
// with a |sectionInfo|.
ContentSuggestionsMostVisitedItem* ConvertNTPTile(
    const ntp_tiles::NTPTile& tile,
    ContentSuggestionsSectionInformation* sectionInfo);

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_
