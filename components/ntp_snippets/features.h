// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_FEATURES_H_
#define COMPONENTS_NTP_SNIPPETS_FEATURES_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/prefs/pref_service.h"

namespace base {
class Clock;
}

namespace ntp_snippets {

// Features to turn individual providers/categories on/off.
// TODO(jkrcal): Rename to kRemoteSuggestionsFeature.
extern const base::Feature kArticleSuggestionsFeature;
extern const base::Feature kBookmarkSuggestionsFeature;
extern const base::Feature kRecentOfflineTabSuggestionsFeature;
extern const base::Feature kPhysicalWebPageSuggestionsFeature;
extern const base::Feature kForeignSessionsSuggestionsFeature;;

// Feature to allow the 'save to offline' option to appear in the snippets
// context menu.
extern const base::Feature kSaveToOfflineFeature;

// Feature to allow offline badges to appear on snippets.
extern const base::Feature kOfflineBadgeFeature;

// Feature to allow UI as specified here: https://crbug.com/660837.
extern const base::Feature kIncreasedVisibility;

// Feature to prefer AMP URLs over regular URLs when available.
extern const base::Feature kPreferAmpUrlsFeature;

// Feature to choose a category ranker.
extern const base::Feature kCategoryRanker;

// Parameter and its values for the kCategoryRanker feature flag.
extern const char kCategoryRankerParameter[];
extern const char kCategoryRankerConstantRanker[];
extern const char kCategoryRankerClickBasedRanker[];

enum class CategoryRankerChoice {
  CONSTANT,
  CLICK_BASED,
};

// Returns which CategoryRanker to use according to kCategoryRanker feature.
CategoryRankerChoice GetSelectedCategoryRanker();

// Builds a CategoryRanker according to kCategoryRanker feature.
std::unique_ptr<CategoryRanker> BuildSelectedCategoryRanker(
    PrefService* pref_service,
    std::unique_ptr<base::Clock> clock);

// Feature to choose a default category order.
extern const base::Feature kCategoryOrder;

// Parameter and its values for the kCategoryOrder feature flag.
extern const char kCategoryOrderParameter[];
extern const char kCategoryOrderGeneral[];
extern const char kCategoryOrderEmergingMarketsOriented[];

enum class CategoryOrderChoice {
  GENERAL,
  EMERGING_MARKETS_ORIENTED,
};

// Returns which category order to use according to kCategoryOrder feature.
CategoryOrderChoice GetSelectedCategoryOrder();

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_FEATURES_H_
