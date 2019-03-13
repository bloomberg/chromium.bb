// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_metrics.h"

#include <algorithm>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"

namespace app_list {

namespace {

int CalculateAnimationSmoothness(int actual_frames,
                                 int ideal_duration_ms,
                                 float refresh_rate) {
  int smoothness = 100;
  const int ideal_frames =
      refresh_rate * ideal_duration_ms / base::Time::kMillisecondsPerSecond;
  if (ideal_frames > actual_frames)
    smoothness = 100 * actual_frames / ideal_frames;
  return smoothness;
}

// These constants affect logging, and  should not be changed without
// deprecating the following UMA histograms:
//  - Apps.AppListTileClickIndexAndQueryLength
//  - Apps.AppListResultClickIndexAndQueryLength
constexpr int kMaxLoggedQueryLength = 10;
constexpr int kMaxLoggedSuggestionIndex = 6;
constexpr int kMaxLoggedHistogramValue =
    (kMaxLoggedSuggestionIndex + 1) * kMaxLoggedQueryLength +
    kMaxLoggedSuggestionIndex;

}  // namespace

// The UMA histogram that logs smoothness of folder show/hide animation.
constexpr char kFolderShowHideAnimationSmoothness[] =
    "Apps.AppListFolder.ShowHide.AnimationSmoothness";

// The UMA histogram that logs smoothness of pagination animation.
constexpr char kPaginationTransitionAnimationSmoothness[] =
    "Apps.PaginationTransition.AnimationSmoothness";

// The UMA histogram that logs which state search results are opened from.
constexpr char kAppListSearchResultOpenSourceHistogram[] =
    "Apps.AppListSearchResultOpenedSource";

// The UMA hisotogram that logs the action user performs on zero state
// search result.
constexpr char kAppListZeroStateSearchResultUserActionHistogram[] =
    "Apps.AppListZeroStateSearchResultUserActionType";

// The UMA histogram that logs user's decision(remove or cancel) for zero state
// search result removal confirmation.
constexpr char kAppListZeroStateSearchResultRemovalHistogram[] =
    "Apps.ZeroStateSearchResutRemovalDecision";

// The UMA histogram that logs the length of the query when user abandons
// results of a queried search or recommendations of zero state(zero length
// query) in launcher UI.
constexpr char kSearchAbandonQueryLengthHistogram[] =
    "Apps.AppListSearchAbandonQueryLength";

// The different sources from which a search result is displayed. These values
// are written to logs.  New enum values can be added, but existing enums must
// never be renumbered or deleted and reused.
enum class ApplistSearchResultOpenedSource {
  kHalfClamshell = 0,
  kFullscreenClamshell = 1,
  kFullscreenTablet = 2,
  kMaxApplistSearchResultOpenedSource = 3,
};

void RecordFolderShowHideAnimationSmoothness(int actual_frames,
                                             int ideal_duration_ms,
                                             float refresh_rate) {
  const int smoothness = CalculateAnimationSmoothness(
      actual_frames, ideal_duration_ms, refresh_rate);
  UMA_HISTOGRAM_PERCENTAGE(kFolderShowHideAnimationSmoothness, smoothness);
}

void RecordPaginationAnimationSmoothness(int actual_frames,
                                         int ideal_duration_ms,
                                         float refresh_rate) {
  const int smoothness = CalculateAnimationSmoothness(
      actual_frames, ideal_duration_ms, refresh_rate);
  UMA_HISTOGRAM_PERCENTAGE(kPaginationTransitionAnimationSmoothness,
                           smoothness);
}

APP_LIST_EXPORT void RecordSearchResultOpenSource(
    const SearchResult* result,
    const AppListModel* model,
    const SearchModel* search_model) {
  // Record the search metric if the SearchResult is not a suggested app.
  if (result->display_type() == ash::SearchResultDisplayType::kRecommendation)
    return;

  ApplistSearchResultOpenedSource source;
  AppListViewState state = model->state_fullscreen();
  if (search_model->tablet_mode()) {
    source = ApplistSearchResultOpenedSource::kFullscreenTablet;
  } else {
    source = state == AppListViewState::HALF
                 ? ApplistSearchResultOpenedSource::kHalfClamshell
                 : ApplistSearchResultOpenedSource::kFullscreenClamshell;
  }
  UMA_HISTOGRAM_ENUMERATION(
      kAppListSearchResultOpenSourceHistogram, source,
      ApplistSearchResultOpenedSource::kMaxApplistSearchResultOpenedSource);
}

void RecordSearchLaunchIndexAndQueryLength(
    SearchResultLaunchLocation launch_location,
    int query_length,
    int suggestion_index) {
  if (suggestion_index < 0) {
    LOG(ERROR) << "Received invalid suggestion index.";
    return;
  }

  query_length = std::min(query_length, kMaxLoggedQueryLength);
  suggestion_index = std::min(suggestion_index, kMaxLoggedSuggestionIndex);
  const int logged_value =
      (kMaxLoggedSuggestionIndex + 1) * query_length + suggestion_index;

  if (launch_location == SearchResultLaunchLocation::kResultList) {
    UMA_HISTOGRAM_EXACT_LINEAR(kAppListResultLaunchIndexAndQueryLength,
                               logged_value, kMaxLoggedHistogramValue);
  } else if (launch_location == SearchResultLaunchLocation::kTileList) {
    UMA_HISTOGRAM_EXACT_LINEAR(kAppListTileLaunchIndexAndQueryLength,
                               logged_value, kMaxLoggedHistogramValue);
  }
}

void RecordSearchAbandonWithQueryLengthHistogram(int query_length) {
  UMA_HISTOGRAM_EXACT_LINEAR(kSearchAbandonQueryLengthHistogram,
                             std::min(query_length, kMaxLoggedQueryLength),
                             kMaxLoggedQueryLength);
}

void RecordZeroStateSearchResultUserActionHistogram(
    ZeroStateSearchResultUserActionType action) {
  UMA_HISTOGRAM_ENUMERATION(kAppListZeroStateSearchResultUserActionHistogram,
                            action);
}

void RecordZeroStateSearchResultRemovalHistogram(
    ZeroStateSearchResutRemovalConfirmation removal_decision) {
  UMA_HISTOGRAM_ENUMERATION(kAppListZeroStateSearchResultRemovalHistogram,
                            removal_decision);
}

}  // namespace app_list
