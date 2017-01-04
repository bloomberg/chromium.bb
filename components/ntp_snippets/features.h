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

// Feature to allow dismissing sections.
extern const base::Feature kSectionDismissalFeature;

// Global toggle for the whole content suggestions feature. If this is set to
// false, all the per-provider features are ignored.
extern const base::Feature kContentSuggestionsFeature;

// Feature to allow UI as specified here: https://crbug.com/660837.
extern const base::Feature kIncreasedVisibility;

// Feature to enable the Fetch More action
extern const base::Feature kFetchMoreFeature;

// Feature to prefer AMP URLs over regular URLs when available.
extern const base::Feature kPreferAmpUrlsFeature;

// Feature to choose a category ranker.
extern const base::Feature kCategoryRanker;

// Parameter for a kCategoryRanker feature flag.
extern const char kCategoryRankerParameter[];
// Possible values of the parameter above.
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

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_FEATURES_H_
