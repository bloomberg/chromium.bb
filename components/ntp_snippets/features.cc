// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/features.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

// Keep sorted, and keep nullptr at the end.
const base::Feature*(kAllFeatures[]) = {&kArticleSuggestionsFeature,
                                        &kBookmarkSuggestionsFeature,
                                        &kCategoryOrder,
                                        &kCategoryRanker,
                                        &kContentSuggestionsPushFeature,
                                        &kForeignSessionsSuggestionsFeature,
                                        &kIncreasedVisibility,
                                        &kKeepPrefetchedContentSuggestions,
                                        &kNotificationsFeature,
                                        &kPhysicalWebPageSuggestionsFeature,
                                        &kPublisherFaviconsFromNewServerFeature,
                                        &kRecentOfflineTabSuggestionsFeature,
                                        nullptr};

const base::Feature kArticleSuggestionsFeature{
    "NTPArticleSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBookmarkSuggestionsFeature{
    "NTPBookmarkSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRecentOfflineTabSuggestionsFeature{
    "NTPOfflinePageSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIncreasedVisibility{"NTPSnippetsIncreasedVisibility",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPhysicalWebPageSuggestionsFeature{
    "NTPPhysicalWebPageSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kForeignSessionsSuggestionsFeature{
    "NTPForeignSessionsSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContentSuggestionsPushFeature{
    "ContentSuggestionsPush", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCategoryRanker{"ContentSuggestionsCategoryRanker",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPublisherFaviconsFromNewServerFeature{
    "ContentSuggestionsFaviconsFromNewServer",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRemoteSuggestionsEmulateM58FetchingSchedule{
    "RemoteSuggestionsEmulateM58FetchingSchedule",
    base::FEATURE_DISABLED_BY_DEFAULT};

const char kCategoryRankerParameter[] = "category_ranker";
const char kCategoryRankerConstantRanker[] = "constant";
const char kCategoryRankerClickBasedRanker[] = "click_based";

CategoryRankerChoice GetSelectedCategoryRanker() {
  std::string category_ranker_value =
      variations::GetVariationParamValueByFeature(kCategoryRanker,
                                                  kCategoryRankerParameter);

  if (category_ranker_value.empty()) {
    // Default, Enabled or Disabled.
    return CategoryRankerChoice::CONSTANT;
  }
  if (category_ranker_value == kCategoryRankerConstantRanker) {
    return CategoryRankerChoice::CONSTANT;
  }
  if (category_ranker_value == kCategoryRankerClickBasedRanker) {
    return CategoryRankerChoice::CLICK_BASED;
  }

  LOG(DFATAL) << "The " << kCategoryRankerParameter << " parameter value is '"
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
  }
  return nullptr;
}

const base::Feature kCategoryOrder{"ContentSuggestionsCategoryOrder",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const char kCategoryOrderParameter[] = "category_order";
const char kCategoryOrderGeneral[] = "general";
const char kCategoryOrderEmergingMarketsOriented[] =
    "emerging_markets_oriented";

CategoryOrderChoice GetSelectedCategoryOrder() {
  if (!base::FeatureList::IsEnabled(kCategoryOrder)) {
    return CategoryOrderChoice::GENERAL;
  }

  std::string category_order_value =
      variations::GetVariationParamValueByFeature(kCategoryOrder,
                                                  kCategoryOrderParameter);

  if (category_order_value.empty()) {
    // Enabled with no parameters.
    return CategoryOrderChoice::GENERAL;
  }
  if (category_order_value == kCategoryOrderGeneral) {
    return CategoryOrderChoice::GENERAL;
  }
  if (category_order_value == kCategoryOrderEmergingMarketsOriented) {
    return CategoryOrderChoice::EMERGING_MARKETS_ORIENTED;
  }

  LOG(DFATAL) << "The " << kCategoryOrderParameter << " parameter value is '"
              << category_order_value << "'";
  return CategoryOrderChoice::GENERAL;
}

const base::Feature kNotificationsFeature = {"ContentSuggestionsNotifications",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const char kNotificationsPriorityParam[] = "priority";
const char kNotificationsTextParam[] = "text";
const char kNotificationsTextValuePublisher[] = "publisher";
const char kNotificationsTextValueSnippet[] = "snippet";
const char kNotificationsTextValueAndMore[] = "and_more";
const char kNotificationsKeepWhenFrontmostParam[] =
    "keep_notification_when_frontmost";
const char kNotificationsOpenToNTPParam[] = "open_to_ntp";
const char kNotificationsDailyLimit[] = "daily_limit";
const char kNotificationsIgnoredLimitParam[] = "ignored_limit";

const base::Feature kKeepPrefetchedContentSuggestions{
    "KeepPrefetchedContentSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace ntp_snippets
