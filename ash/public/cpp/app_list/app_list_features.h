// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace base {
struct Feature;
}

namespace app_list_features {

// Please keep these features sorted.
// TODO(newcomer|weidongg): Sort these features.

// Enables the answer card in the app list.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAnswerCard;

// Enables background blur for the app list, lock screen, and tab switcher, also
// enables the AppsGridView mask layer. In this mode, slower devices may have
// choppier app list animations. crbug.com/765292.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableBackgroundBlur;

// Enables the Play Store app search.
ASH_PUBLIC_EXPORT extern const base::Feature kEnablePlayStoreAppSearch;

// Enables in-app data search.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppDataSearch;

// Enables the Settings shortcut search.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableSettingsShortcutSearch;

// Enables the feature to display zero state suggestions.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableZeroStateSuggestions;

// Enables the feature to autocomplete text typed in the AppList search box.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppListSearchAutocomplete;

// Enable app ranking models.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppRanker;

// Enable an model that ranks zero-state apps search result.
// TODO(crbug.com/989350): This flag can be removed once the
// AppSearchResultRanker is removed. Same with the
// AppSearchResultRankerPredictorName.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableZeroStateAppsRanker;

// Enable an model that ranks query based non-apps result.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableQueryBasedMixedTypesRanker;

// Enable a model that ranks zero-state files and recent queries.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableZeroStateMixedTypesRanker;

// Enables the feature to include a single reinstallation candidate in
// zero-state.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppReinstallZeroState;

// Enables the embedded Assistant UI in the app list.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableEmbeddedAssistantUI;

// Enables ghosting in any AppsGridView (folder or root) when dragging an item.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppGridGhost;

// Enables hashed recording of a app list launches.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppListLaunchRecording;

ASH_PUBLIC_EXPORT extern const base::Feature kEnableSearchBoxSelection;

// Enables using the aggregated Ml model to rank suggested apps.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAggregatedMlAppRanking;

// If enabled, app list will support separate configurations (for app list items
// sizing and spacing) for smaller screens (instead of a single configuration
// that optionally gets scaled down).
ASH_PUBLIC_EXPORT extern const base::Feature kScalableAppList;

bool ASH_PUBLIC_EXPORT IsAnswerCardEnabled();
bool ASH_PUBLIC_EXPORT IsBackgroundBlurEnabled();
bool ASH_PUBLIC_EXPORT IsPlayStoreAppSearchEnabled();
bool ASH_PUBLIC_EXPORT IsAppDataSearchEnabled();
bool ASH_PUBLIC_EXPORT IsSettingsShortcutSearchEnabled();
bool ASH_PUBLIC_EXPORT IsZeroStateSuggestionsEnabled();
bool ASH_PUBLIC_EXPORT IsAppListSearchAutocompleteEnabled();
bool ASH_PUBLIC_EXPORT IsAppRankerEnabled();
bool ASH_PUBLIC_EXPORT IsZeroStateAppsRankerEnabled();
bool ASH_PUBLIC_EXPORT IsQueryBasedMixedTypesRankerEnabled();
bool ASH_PUBLIC_EXPORT IsZeroStateMixedTypesRankerEnabled();
bool ASH_PUBLIC_EXPORT IsAppReinstallZeroStateEnabled();
bool ASH_PUBLIC_EXPORT IsEmbeddedAssistantUIEnabled();
bool ASH_PUBLIC_EXPORT IsAppGridGhostEnabled();
bool ASH_PUBLIC_EXPORT IsAppListLaunchRecordingEnabled();
bool ASH_PUBLIC_EXPORT IsSearchBoxSelectionEnabled();
bool ASH_PUBLIC_EXPORT IsAggregatedMlAppRankingEnabled();
bool ASH_PUBLIC_EXPORT IsScalableAppListEnabled();

std::string ASH_PUBLIC_EXPORT AnswerServerUrl();
std::string ASH_PUBLIC_EXPORT AnswerServerQuerySuffix();
std::string ASH_PUBLIC_EXPORT AppSearchResultRankerPredictorName();

}  // namespace app_list_features

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
