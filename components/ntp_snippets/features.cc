// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/features.h"

#include "base/time/clock.h"
#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

const base::Feature kArticleSuggestionsFeature{
    "NTPArticleSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBookmarkSuggestionsFeature{
    "NTPBookmarkSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRecentOfflineTabSuggestionsFeature{
    "NTPOfflinePageSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSaveToOfflineFeature{
    "NTPSaveToOffline", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOfflineBadgeFeature{
    "NTPOfflineBadge", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kIncreasedVisibility{
    "NTPSnippetsIncreasedVisibility", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPhysicalWebPageSuggestionsFeature{
    "NTPPhysicalWebPageSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kContentSuggestionsFeature{
    "NTPSnippets", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSectionDismissalFeature{
    "NTPSuggestionsSectionDismissal", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kForeignSessionsSuggestionsFeature{
    "NTPForeignSessionsSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kFetchMoreFeature{"NTPSuggestionsFetchMore",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPreferAmpUrlsFeature{"NTPPreferAmpUrls",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCategoryRanker{"ContentSuggestionsCategoryRanker",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const char kCategoryRankerParameter[] = "category_ranker";
const char kCategoryRankerConstantRanker[] = "constant";
const char kCategoryRankerClickBasedRanker[] = "click_based";

CategoryRankerChoice GetSelectedCategoryRanker() {
  std::string category_ranker_value =
      variations::GetVariationParamValueByFeature(kCategoryRanker,
                                                  kCategoryRankerParameter);

  if (category_ranker_value.empty()) {
    // Default, Enabled or Disabled.
    return CategoryRankerChoice::CLICK_BASED;
  }
  if (category_ranker_value == kCategoryRankerConstantRanker) {
    return CategoryRankerChoice::CONSTANT;
  }
  if (category_ranker_value == kCategoryRankerClickBasedRanker) {
    return CategoryRankerChoice::CLICK_BASED;
  }

  NOTREACHED() << "The " << kCategoryRankerParameter << " parameter value is '"
               << category_ranker_value << "'";
  return CategoryRankerChoice::CONSTANT;
}

std::unique_ptr<CategoryRanker> BuildSelectedCategoryRanker(
    PrefService* pref_service,
    std::unique_ptr<base::Clock> clock) {
  CategoryRankerChoice choice = ntp_snippets::GetSelectedCategoryRanker();
  switch (choice) {
    case CategoryRankerChoice::CONSTANT:
      return base::MakeUnique<ConstantCategoryRanker>();
    case CategoryRankerChoice::CLICK_BASED:
      return base::MakeUnique<ClickBasedCategoryRanker>(pref_service,
                                                        std::move(clock));
    default:
      NOTREACHED() << "The category ranker choice value is "
                   << static_cast<int>(choice);
  }
  return nullptr;
}

}  // namespace ntp_snippets
